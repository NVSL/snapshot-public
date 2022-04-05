SOURCES := $(wildcard *.cc)
OBJECTS := $(patsubst %.cc, %.o, $(SOURCES))
DEPENDS := $(wildcard ../../include/*)
INCLUDE :=-iquote../../include
INCLUDE +=-iquote../

ifndef LLVM_DIR
$(error "LLVM_DIR not set")
endif

DIR      := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))
LIBS_DIR := $(shell readlink -f $(DIR)../../../lib)
_LIBS_DIR := $(shell readlink -f $(DIR))
PUDDLE_CXXFLAGS += -std=c++20
EXTRA_LDFLAGS +=-L$(LIBS_DIR) -Wl,-R$(LIBS_DIR) -Wl,--no-as-needed
EXTRA_LDFLAGS +=-L$(_LIBS_DIR) -Wl,-R$(_LIBS_DIR) -Wl,--no-as-needed
EXTRA_LINKFLAGS +=-luuid -lpthread -mclwb -ldl 
EXTRA_LINKFLAGS +=-Wl,--whole-archive
EXTRA_LINKFLAGS +=$(LIBS_DIR)/libstoreinst.a 
EXTRA_LINKFLAGS +=-Wl,--no-whole-archive

CXX = $(LLVM_DIR)/bin/clang++
CC = $(LLVM_DIR)/bin/clang
DXX = ../../storeinst-pass/dclang++

include ../../common.make

all:
	$(PUDDLES_MAKE) simplekv_puddles_redir

.PHONY: simplekv_puddles_redir
simplekv_puddles_redir: simplekv_puddles simplekv_puddles.inst simplekv_puddles.no_sync

simplekv_puddles: simplekv_puddles.o
	$(PUDDLES_CXX)  $(CXXFLAGS) $(LDFLAGS) $(LINKFLAGS) $^ -o $@

%.o.no_sync: %.cc $(DEPS_DIR)/%.d | $(DEPS_DIR)
	$(PUDDLES_CXX) -DDONT_SYNC $(INCLUDE) -mclwb -mavx -mavx2 -mavx512f -c $(CXXFLAGS)\
		$(PUDDLES_CXXFLAGS) $< -MT $@ -o $@ -MMD -MP -MF $(DEPS_DIR)/$*.d

simplekv_puddles.no_sync: simplekv_puddles.o.no_sync
	$(PUDDLES_CXX)  $(CXXFLAGS) $(LDFLAGS) $(LINKFLAGS) $^ -o $@

simplekv_puddles.inst: simplekv_puddles.cc
	$(PUDDLES_DXX) $(INCLUDE) $(CXXFLAGS) $(LDFLAGS) -luuid -lpthread -mclwb -ldl  $^ -o $@

clean: clean_deps
	-rm -f *.a *.o *.bc *.so *.inst simplekv_puddles

include ../../deps.make
