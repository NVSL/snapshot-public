#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/FunctionPropertiesAnalysis.h"
#include "llvm/Analysis/GlobalsModRef.h"
#include "llvm/Analysis/MemoryLocation.h"
#include "llvm/Analysis/ScopedNoAliasAA.h"
#include "llvm/Analysis/TypeBasedAliasAnalysis.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/LegacyPassManagers.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/PassRegistry.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <cstdarg>
#include <cxxabi.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/TargetTransformInfo.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <unordered_map>

#include <llvm/Pass.h>
#include <sstream>

using namespace llvm;

#define COLOR "\e[0;34m"
#define END "\e[0m"

namespace {

size_t modCount = 0, skipCount = 0, skipFn = 0, aliasedLocationFound = 0,
       exactLocationMatchFound = 0;

static void log(char *msg, ...) {
  va_list(arg);
  va_start(arg, msg);
  char *buf;
  vasprintf(&buf, msg, arg);
  fprintf(stderr, COLOR "SIP: %s\n" END, buf);
  free(buf);
  va_end(arg);
}

static FunctionCallee createRuntimeCheckFunc(Module &m) {
  Type *VoidType = Type::getVoidTy(m.getContext());
  Type *VoidPtrType = Type::getInt8PtrTy(m.getContext());
  FunctionType *FuncType =
      FunctionType::get(VoidType, ArrayRef<Type *>(VoidPtrType), false);
  return m.getOrInsertFunction("checkMemory", FuncType);
}

std::string demangleSym(const std::string &mangledName) {
  int status;
  const auto demangledName =
      abi::__cxa_demangle(mangledName.c_str(), nullptr, nullptr, &status);

  return (status == 0) ? demangledName : mangledName;
}

/** @brief Check if the store instruction aliases to any of the stack ptrs */
bool writesToStackLocation(AAResults &aa, const StoreInst *si,
                           std::unordered_map<Value *, bool> &stackValues) {
  bool result = false;
  const auto instValue = si->getOperand(1);

  for (auto &[possibleStackLoc, isLocOnStack] : stackValues) {
    if (isLocOnStack) {
      const auto stackValue = possibleStackLoc;
      if (stackValue == instValue) {
        exactLocationMatchFound++;
        result = true;
        break;
      } else if (aa.alias(stackValue, instValue) ==
                 llvm::AliasResult::MustAlias) {
        aliasedLocationFound++;
        result = true;
        break;
      }
    }
  }

  return result;
}

void analyseFunc(AAResults &aa, Function &f, FunctionCallee &checkMemory) {
  std::unordered_map<Value *, bool> stackValues;

  for (Function::iterator bi = f.begin(); bi != f.end(); bi++) {
    for (BasicBlock::iterator it = bi->begin(); it != bi->end(); it++) {
      Instruction *i = &*it;

      if (StoreInst *si = dyn_cast<StoreInst>(i)) {
        Value *ptr = si->getPointerOperand();

        /* Don't instrument stack operations */
        if (not writesToStackLocation(aa, si, stackValues)) {
          LLVMContext &c = si->getContext();
          ptr = new BitCastInst(ptr, Type::getInt8PtrTy(c), ptr->getName(), si);

          CallInst::Create(checkMemory, ArrayRef<Value *>(ptr), "", si);
          modCount++;
        } else {
          skipCount++;
        }
      } else if (AllocaInst *ai = dyn_cast<AllocaInst>(i)) {
        stackValues[ai] = true;
      }
    }
  }
}

// New PM implementation
struct HelloWorld : PassInfoMixin<HelloWorld> {
  AliasAnalysis *aa;

  HelloWorld(void *ptr) {}
  PreservedAnalyses run(Module &m, ModuleAnalysisManager &mam) {
    FunctionCallee checkMemory = createRuntimeCheckFunc(m);
    FunctionAnalysisManager &fam =
        mam.getResult<FunctionAnalysisManagerModuleProxy>(m).getManager();

    assert(not fam.empty());

    for (auto fi = m.begin(); fi != m.end(); ++fi) {
      if (not fi->empty()) {
        auto demangledName = demangleSym(fi->getName().str());
        auto &aam = fam.getResult<AAManager>(*fi);
        analyseFunc(aam, *fi, checkMemory);
      }
    }

    log((char *)"Instrumented %lu, skipped %lu locations (aliased=%lu, "
                "exact=%lu).",
        modCount, skipCount, aliasedLocationFound, exactLocationMatchFound);
    return PreservedAnalyses::all();
  }
};

// Legacy PM implementation
struct LegacyHelloWorld : public ModulePass {
  static char ID;
  LegacyHelloWorld() : ModulePass(ID) {
    errs() << COLOR "Legacy mode not supported" END "\n";
    exit(1);
  }
  // Main entry point - the name conveys what unit of IR this is to be run on.
  bool runOnModule(Module &m) override {
    // Doesn't modify the input unit of IR, hence 'false'
    return false;
  }
};
} // namespace

// New PM interface
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "HelloWorld", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineStartEPCallback(
                [&](llvm::ModulePassManager &mpm,
                    llvm::PassBuilder::OptimizationLevel o) -> void {
                  mpm.addPass(createModuleToFunctionPassAdaptor(
                      RequireAnalysisPass<ScopedNoAliasAA, Function>()));
                  mpm.addPass(createModuleToFunctionPassAdaptor(
                      RequireAnalysisPass<TypeBasedAA, Function>()));
                  mpm.addPass(createModuleToFunctionPassAdaptor(
                      RequireAnalysisPass<BasicAA, Function>()));
                  mpm.addPass(HelloWorld(nullptr));
                });
          }};
}

// Old PM interface, we don't support it
char LegacyHelloWorld::ID = 0;

static RegisterPass<LegacyHelloWorld>
    X("legacy-hello-world", "Hello World Pass",
      true, // This pass doesn't modify the CFG => true
      false // This pass is not a pure analysis pass => false
    );

static void registerPass(const llvm::PassManagerBuilder &,
                         legacy::PassManagerBase &PM) {
  PM.add(new LegacyHelloWorld());
}
