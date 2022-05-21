SOURCES := $(wildcard *.cc)
OBJECTS := $(patsubst %.cc, %.o, $(SOURCES))
DEPENDS := $(wildcard ../../include/*)
INCLUDE :=-iquote../../include

DIR      := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))
LIBS_DIR := $(shell readlink -f $(DIR)../../../lib)
EXTRA_LDFLAGS +=-L$(LIBS_DIR) -Wl,-R$(LIBS_DIR) -Wl,--no-as-needed -pthread

ifndef LLVM_DIR
$(error "LLVM_DIR not set")
endif

CXX = $(LLVM_DIR)/bin/clang++
CC = $(LLVM_DIR)/bin/clang
DXX = ../../storeinst-pass/dclang++
CXX=$(DXX)
CC=$(DXX)

include ../../common.make

all: run

run: $(OBJECTS)
	$(PUDDLES_DXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LINKFLAGS)

clean: clean_deps
	-rm -f *.o run

include ../../deps.make
