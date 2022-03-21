//=============================================================================
// FILE:
//    HelloWorld.cpp
//
// DESCRIPTION:
//    Visits all functions in a module, prints their names and the number of
//    arguments via stderr. Strictly speaking, this is an analysis pass (i.e.
//    the functions are not modified). However, in order to keep things simple
//    there's no 'print' method here (every analysis pass should implement it).
//
// USAGE:
//    1. Legacy PM
//      opt -load libHelloWorld.dylib -legacy-hello-world -disable-output `\`
//        <input-llvm-file>
//    2. New PM
//      opt -load-pass-plugin=libHelloWorld.dylib -passes="hello-world" `\`
//        -disable-output <input-llvm-file>
//
//
// License: MIT
//=============================================================================
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/PassRegistry.h"
#include "llvm/IR/LegacyPassManagers.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include <llvm/IR/Constant.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/BasicBlock.h>

#include <llvm/Pass.h>
#include <sstream>

using namespace llvm;

#define COLOR "\e[0;34m"
#define END "\e[0m"

//-----------------------------------------------------------------------------
// HelloWorld implementation
//-----------------------------------------------------------------------------
// No need to expose the internals of the pass to the outside world - keep
// everything in an anonymous namespace.
namespace {

  // This method implements what the pass does
  void visitor(Function &F) {
    // std::cerr << "alkdslfk" << std::endl;
    errs() << "(llvm-tutor) Hello from: "<< F.getName() << "\n";
    errs() << "(llvm-tutor)   number of arguments: " << F.arg_size() << "\n";
  }

  static FunctionCallee
  createRuntimeCheckFunc(Module &m) {
    Type *VoidType = Type::getVoidTy(m.getContext());
    Type *VoidPtrType = Type::getInt8PtrTy(m.getContext());
    FunctionType *FuncType = FunctionType::get(VoidType,
                                               ArrayRef<Type*>(VoidPtrType),
                                               false);
    return m.getOrInsertFunction("checkMemory", FuncType);
  }

  std::map<std::string, bool> stackValues;
  
  void analysis(Module &m) {
    FunctionCallee checkMemory = createRuntimeCheckFunc(m);

    size_t mod_count = 0, skip_count = 0, skip_fn = 0;

    /* Annotate all the functions */
    auto global_annos = m.getNamedGlobal("llvm.global.annotations");
    if (global_annos) {
      auto a  = cast<ConstantArray>(global_annos->getOperand(0));

      for (int i = 0; i < a->getNumOperands(); i++) {
        auto e = cast<ConstantStruct>(a->getOperand(i));

        if (auto fn = dyn_cast<Function>(e->getOperand(0)->getOperand(0))) {
          auto gv = cast<GlobalVariable>(e->getOperand(1)->getOperand(0));
          auto anno = cast<ConstantDataArray>(gv->getOperand(0))->getAsCString();
          fn->addFnAttr(anno);
        }
      }
    }

    /* Go over all the store instructions */
    for (Module::iterator fi = m.begin(); fi != m.end(); ++fi) {
      if (not fi->hasFnAttribute("do_not_auto_log")) {
        for (Function::iterator bi = fi->begin(); bi != fi->end(); bi++) {
          for (BasicBlock::iterator it = bi->begin(); it != bi->end(); it++) {
            Instruction *i = &*it;

            if (StoreInst *si = dyn_cast<StoreInst>(i)) {
              Value *ptr = si->getPointerOperand();

              std::stringstream ss;
              ss << ptr;
            
              /* Don't instrument stack operations */
              if (not stackValues[ss.str()]) {
                LLVMContext &c= si->getContext();
                ptr = new BitCastInst(ptr, Type::getInt8PtrTy(c), ptr->getName(), si);

                CallInst::Create(checkMemory, ArrayRef<Value*>(ptr), "", si);
                mod_count++;
              } else {
                skip_count++;
              }
            } else if (AllocaInst *ai = dyn_cast<AllocaInst>(i)) {
              std::stringstream ss;
              ss << ai;
              stackValues[ss.str()] = true;
            }
          }
        }
      } else {
        skip_fn++;
        errs() << COLOR "SIP: Found function '" << fi->getName()
               << "' with annotation, skipping..." END << "\n";               
      }
    }

    errs() << COLOR "SIP: Instrumented " << mod_count << " and skipped " << skip_count
           << " locations" << END "\n";
    errs() << COLOR "SIP: " << skip_fn << " functions skipped" << END "\n";

  }

  // New PM implementation
  struct HelloWorld : PassInfoMixin<HelloWorld> {
    HelloWorld() {
      
    }
    // Main entry point, takes IR unit to run the pass on (&F) and the
    // corresponding pass manager (to be queried if need be)
    PreservedAnalyses run(Module &m, ModuleAnalysisManager &) {
      analysis(m);
      return PreservedAnalyses::all();
    }
  };

  // Legacy PM implementation
  struct LegacyHelloWorld : public ModulePass {
    static char ID;
    LegacyHelloWorld() : ModulePass(ID) {
      errs() << "Legacy mode not supported" << "\n";
    }
    // Main entry point - the name conveys what unit of IR this is to be run on.
    bool runOnModule(Module &m) override {
      analysis(m);
      // Doesn't modify the input unit of IR, hence 'false'
      return false;
    }
  };
} // namespace

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getHelloWorldPluginInfo_old() {
  return {LLVM_PLUGIN_API_VERSION, "HelloWorld", LLVM_VERSION_STRING,
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
                                         [](StringRef Name, ModulePassManager &FPM,
                                            ArrayRef<PassBuilder::PipelineElement>) {
                                           if (Name == "hello-world") {
                                             FPM.addPass(HelloWorld());
                                             return true;
                                           }
                                           return false;
                                         });
    }};
}
llvm::PassPluginLibraryInfo getHelloWorldPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "HelloWorld", LLVM_VERSION_STRING,
    [](PassBuilder &PB) {
      PB.registerPipelineStartEPCallback
        (
         [&](llvm::ModulePassManager &mpm, llvm::PassBuilder::OptimizationLevel o) -> void{
           mpm.addPass(HelloWorld());
         }
         );
    }};
}

// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize HelloWorld when added to the pass pipeline on the
// command line, i.e. via '-passes=hello-world'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getHelloWorldPluginInfo();
}

//-----------------------------------------------------------------------------
// Legacy PM Registration
//-----------------------------------------------------------------------------
// The address of this variable is used to uniquely identify the pass. The
// actual value doesn't matter.
char LegacyHelloWorld::ID = 0;

// This is the core interface for pass plugins. It guarantees that 'opt' will
// recognize LegacyHelloWorld when added to the pass pipeline on the command
// line, i.e.  via '--legacy-hello-world'
static RegisterPass<LegacyHelloWorld>
    X("legacy-hello-world", "Hello World Pass",
      true, // This pass doesn't modify the CFG => true
      false // This pass is not a pure analysis pass => false
    );

static void registerPass(const llvm::PassManagerBuilder&,
                         legacy::PassManagerBase &PM) {
  PM.add(new LegacyHelloWorld());
}
