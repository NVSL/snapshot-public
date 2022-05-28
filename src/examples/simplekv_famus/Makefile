SOURCES := $(wildcard *.cc)
OBJECTS := $(patsubst %.cc, %.o, $(SOURCES))
DEPENDS := $(wildcard ../../include/*)
INCLUDE :=-iquote../../include
INCLUDE +=-iquote../

DIR      := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))
LIBS_DIR := $(shell readlink -f $(DIR)../../../lib)
_LIBS_DIR := $(shell readlink -f $(DIR))
PUDDLE_CXXFLAGS += -std=c++20
EXTRA_LDFLAGS +=-L$(LIBS_DIR) -Wl,-R$(LIBS_DIR) -Wl,--no-as-needed
EXTRA_LDFLAGS +=-L$(_LIBS_DIR) -Wl,-R$(_LIBS_DIR) -Wl,--no-as-needed
EXTRA_LINKFLAGS +=-luuid -lpthread

CXX = $(LLVM_DIR)/bin/clang++
CC = $(LLVM_DIR)/bin/clang
DXX = ../../storeinst-pass/dclang++

include ../../common.make

all:
	$(PUDDLES_MAKE) simplekv_famus_redir

.PHONY: simplekv_famus_redir
simplekv_famus_redir: simplekv_famus

simplekv_famus: simplekv_famus.o
	$(PUDDLES_CXX)  $(CXXFLAGS) $(LDFLAGS) $(LINKFLAGS) $^ -o $@

clean: clean_deps
	-rm -f *.a *.o *.bc *.so *.inst simplekv_famus

include ../../deps.make
