.PHONY: simplekv_puddles
SOURCES := $(wildcard *.cc)
OBJECTS := $(patsubst %.cc, %.o, $(SOURCES))
INCLUDE :=-I../include

DIR      := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))
LIBS_DIR := $(shell readlink -f $(DIR)../../lib)
EXTRA_LDFLAGS +=-L$(LIBS_DIR) -Wl,-R$(LIBS_DIR) -Wl,--no-as-needed
EXTRA_LINKFLAGS +=-ldsaemu -pthread -lpmbuffer

include ../common.make

all: dummy cat-demo periodic-msync simplekv_puddles

simplekv_puddles:
	$(PUDDLES_MAKE) -C simplekv_puddles

dummy: dummy.o
	$(PUDDLES_CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LINKFLAGS)

cat-demo: cat-demo.o
	$(PUDDLES_CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LINKFLAGS)

periodic-msync: periodic-msync.o
	$(PUDDLES_CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^


clean: clean_deps 
	$(PUDDLES_MAKE) -C simplekv_puddles clean
	$(PUDDLES_MAKE) -C kelly_hoh_code clean
	-rm -rf *.o dummy cat-demo periodic-msync

include ../deps.make
