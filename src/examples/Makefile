.PHONY: simplekv_cxlbuf simplekv_pmdk btree map linkedlist_tx
SOURCES := $(wildcard *.cc)
OBJECTS := $(patsubst %.cc, %.o, $(SOURCES))
INCLUDE :=-I../include

DIR      := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))
LIBS_DIR := $(shell readlink -f $(DIR)../../lib)
EXTRA_LDFLAGS +=-L$(LIBS_DIR) -Wl,-R$(LIBS_DIR) -Wl,--no-as-needed
EXTRA_LINKFLAGS +=-ldsaemu -pthread -lpmbuffer

SUBDIRS := simplekv_cxlbuf simplekv_pmdk linkedlist_pmdk linkedlist_tx kelly_hoh_code

include ../common.make

all: dummy cat-demo periodic-msync $(addsuffix _subdir,$(SUBDIRS)) map

map: btree_subdir
	$(PUDDLES_MAKE) -C map

dummy: dummy.o
	$(PUDDLES_CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LINKFLAGS)

cat-demo: cat-demo.o
	$(PUDDLES_CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LINKFLAGS)

periodic-msync: periodic-msync.o
	$(PUDDLES_CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

%_subdir:
	$(PUDDLES_MAKE) -C $(subst _subdir,,$@)

%_subdir_clean:
	$(PUDDLES_MAKE) -C $(subst _subdir_clean,,$@) clean


clean: clean_deps map_subdir_clean $(addsuffix _subdir_clean,$(SUBDIRS))
	-rm -rf *.o dummy cat-demo periodic-msync

include ../deps.make
