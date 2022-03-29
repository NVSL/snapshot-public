SOURCES := $(wildcard *.cc)
OBJECTS := $(patsubst %.cc, %.o, $(SOURCES))
DEPENDS := $(wildcard ../../include/*)
INCLUDE :=-iquote../../include -iquote../map

DIR      := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))
LIBS_DIR := $(shell readlink -f $(DIR)../../../lib)
EXTRA_LDFLAGS +=-L$(LIBS_DIR) -Wl,-R$(LIBS_DIR)

# Change CC and CXX before including common to automatically configure
# PUDDLES_CXX and PUDDLES_CC
export CC=../../storeinst-pass/dclang
export CXX=../../storeinst-pass/dclang++

include ../../common.make

all: btree.o

btree_test: $(OBJECTS)
	$(PUDDLES_CXX) $(CXXFLAGS) -o $@ $(MAPS_OBJS) $^ $(LDFLAGS)

clean: clean_deps
	-rm -f *.o btree_test

include ../../deps.make
