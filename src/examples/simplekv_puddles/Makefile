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
EXTRA_LINKFLAGS +=-luuid -lpthread -lstoreinst -mclwb -ldl -Wl,--whole-archive libstoreinst.a -Wl,--no-whole-archive

CXX = $(LLVM_DIR)/bin/clang++
CC = $(LLVM_DIR)/bin/clang
DXX = ../../storeinst-pass/dclang++

include ../../common.make

all: libstoreinst.a libstoreinst.so
	$(PUDDLES_MAKE) simplekv_puddles_redir

.PHONY: simplekv_puddles_redir
simplekv_puddles_redir: simplekv_puddles simplekv_puddles.inst

simplekv_puddles: simplekv_puddles.o
	$(PUDDLES_CXX)  $(CXXFLAGS) $(LDFLAGS) $(LINKFLAGS) $^ -o $@

simplekv_puddles.inst: simplekv_puddles.cc
	$(PUDDLES_DXX) $(INCLUDE) $(CXXFLAGS) $(LDFLAGS) $(LINKFLAGS) $^ -o $@

libstoreinst.o: libstoreinst.cc
	$(PUDDLES_CXX) $(INCLUDE) $(CXXFLAGS) -O0 -c $^ -o $@ -mclwb

libstoreinst.a: libstoreinst.o
	$(PUDDLES_AR) rcs $@ $^

libstoreinst.so: libstoreinst.o
	$(PUDDLES_CXX) $(CXXFLAGS) -shared -O0 -o $@ $<

clean: clean_deps
	-rm -f *.a *.o *.bc *.so *.inst simplekv_puddles

include ../../deps.make
