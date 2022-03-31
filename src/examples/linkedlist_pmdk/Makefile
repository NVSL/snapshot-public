SOURCES := $(wildcard *.cc)
OBJECTS := $(patsubst %.cc, %.o, $(SOURCES))

DIR      := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))
LIBS_DIR := $(shell readlink -f $(DIR)../../../lib)
EXTRA_LDFLAGS +=-L$(LIBS_DIR) -Wl,-R$(LIBS_DIR) -Wl,--no-as-needed
EXTRA_LINKFLAGS +=-lpmemobj
INCLUDE := -iquote../../include
include ../../common.make

all: linkedlist_pmdk

linkedlist_pmdk: linkedlist_pmdk.o
	$(PUDDLES_CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $< $(LINKFLAGS)

clean: clean_deps
	-rm -f *.o linkedlist_pmdk

include ../../deps.make
