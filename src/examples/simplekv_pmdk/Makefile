# SOURCES := $(wildcard *.cc)
# OBJECTS := $(patsubst %.cc, %.o, $(SOURCES))
# DEPENDS := $(wildcard ../../include/*)
# # DEPENDS += $(wildcard ./*.hh)
INCLUDE :=-iquote../../include
INCLUDE +=-iquote../../

DIR      := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))
LIBS_DIR := $(shell readlink -f $(DIR)../../../lib)

include ../../common.make

all: simplekv_pmdk

release: all

simplekv_pmdk: simplekv_pmdk.cc simplekv_pmdk.hh
	$(PUDDLES_CXX) -O3 -g -std=c++20 $(INCLUDE) -o simplekv_pmdk simplekv_pmdk.cc -lpmemobj $(EXTRA_LDFLAGS)

clean:
	-rm -f *.o simplekv_pmdk

include ../../deps.make
