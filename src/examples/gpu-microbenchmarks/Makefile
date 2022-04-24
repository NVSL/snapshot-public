SOURCES := $(wildcard *.cc)
OBJECTS := $(patsubst %.cc, %.o, $(SOURCES))
DEPENDS := $(wildcard ../../include/*)
# DEPENDS += $(wildcard ./*.hh)
INCLUDE :=-iquote../../include
# INCLUDE +=-iquote../../

DIR      := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))
LIBS_DIR := $(shell readlink -f $(DIR)../../../lib)
EXTRA_LDFLAGS +=-L$(LIBS_DIR) -Wl,-R$(LIBS_DIR) -Wl,--no-as-needed
EXTRA_LINKFLAGS +=-lrt -luuid -lpthread -rdynamic -lvram

include ../../common.make

all: fbtest

fbtest: fbtest.o
	$(PUDDLES_CXX) $(INCLUDE) $(CXXFLAGS) $(LDFLAGS) -mno-avx512f $<  -g -iquote"../../../vendor/cpp-common/include" -o $@ $(LINKFLAGS)

clean: clean_deps
	-rm -f *.o microbenchmarks

include ../../deps.make
