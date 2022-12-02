.PHONY: simplekv_cxlbuf simplekv_pmdk simplekv_famus btree map map_pmdk	\
	linkedlist_tx linkedlist_cxlfs single_write microbenchmarks redirect		\
	microbenchmarks_cxlbuf microbenchmarks_famus

SOURCES := $(wildcard *.cc)
OBJECTS := $(patsubst %.cc, %.o, $(SOURCES))
INCLUDE :=-I../include

DIR      := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))
LIBS_DIR := $(shell readlink -f $(DIR)../../lib)
EXTRA_CXXFLAGS += -fno-omit-frame-pointer
EXTRA_LDFLAGS +=-L$(LIBS_DIR) -Wl,-R$(LIBS_DIR) -Wl,--no-as-needed
EXTRA_LINKFLAGS +=-ldsaemu -pthread -lpmbuffer

SUBDIRS := map_pmdk simplekv_cxlbuf simplekv_pmdk simplekv_famus		\
	linkedlist_pmdk linkedlist_tx linkedlist_cxlfs microbenchmarks gpu-microbenchmarks	\
	microbenchmarks_cxlbuf microbenchmarks_famus

include ../common.make

all: 
	$(PUDDLES_MAKE) -C redirect
	$(PUDDLES_MAKE) depends

depends: single_write dummy cat-demo periodic-msync $(addsuffix _subdir,$(SUBDIRS)) \
	map map_pmdk 

map: btree_subdir
	$(PUDDLES_MAKE) -C map

dummy: dummy.o
	$(PUDDLES_CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LINKFLAGS)

single_write:
	../storeinst-pass/dclang++ -L$(LIBS_DIR) -Wl,-R$(LIBS_DIR) -Wl,--no-as-needed -std=c++20 -ldl -lstoreinst $@.cc -o $@

cat-demo: cat-demo.o
	$(PUDDLES_CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LINKFLAGS)

periodic-msync: periodic-msync.o
	$(PUDDLES_CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

%_subdir:
	$(PUDDLES_MAKE) -C $(subst _subdir,,$@)

%_subdir_clean:
	$(PUDDLES_MAKE) -C $(subst _subdir_clean,,$@) clean


clean: clean_deps map_subdir_clean $(addsuffix _subdir_clean,$(SUBDIRS)) btree_subdir_clean
	-rm -rf *.o dummy cat-demo periodic-msync

include ../deps.make
