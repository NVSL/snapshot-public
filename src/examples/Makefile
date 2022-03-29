.PHONY: simplekv_cxlbuf simplekv_pmdk btree map
SOURCES := $(wildcard *.cc)
OBJECTS := $(patsubst %.cc, %.o, $(SOURCES))
INCLUDE :=-I../include

DIR      := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))
LIBS_DIR := $(shell readlink -f $(DIR)../../lib)
EXTRA_LDFLAGS +=-L$(LIBS_DIR) -Wl,-R$(LIBS_DIR) -Wl,--no-as-needed
EXTRA_LINKFLAGS +=-ldsaemu -pthread -lpmbuffer

include ../common.make

all: dummy cat-demo periodic-msync simplekv_cxlbuf simplekv_pmdk map btree

map: btree
	$(PUDDLES_MAKE) -C map

btree:
	$(PUDDLES_MAKE) -C btree

simplekv_cxlbuf simplekv_pmdk:
	$(PUDDLES_MAKE) -C $@

dummy: dummy.o
	$(PUDDLES_CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LINKFLAGS)

cat-demo: cat-demo.o
	$(PUDDLES_CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LINKFLAGS)

periodic-msync: periodic-msync.o
	$(PUDDLES_CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^


clean: clean_deps 
	$(PUDDLES_MAKE) -C simplekv_cxlbuf clean
	$(PUDDLES_MAKE) -C simplekv_pmdk clean
	$(PUDDLES_MAKE) -C kelly_hoh_code clean
	$(PUDDLES_MAKE) -C map clean
	$(PUDDLES_MAKE) -C btree clean
	-rm -rf *.o dummy cat-demo periodic-msync

include ../deps.make
