SOURCES := $(wildcard *.cc)
OBJECTS := $(patsubst %.cc, %.o, $(SOURCES))
DEPENDS := $(wildcard ../../include/*)
INCLUDE :=-iquote../../include

DIR      := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))
LIBS_DIR := $(shell readlink -f $(DIR)../../../lib)
EXTRA_LDFLAGS +=-L$(LIBS_DIR) -Wl,-R$(LIBS_DIR) -Wl,--no-as-needed
EXTRA_LINKFLAGS +=-lrt -luuid -lpthread -rdynamic -lvram -lcxlfs
EXTRA_LINKFLAGS +=-Wl,--exclude-libs,ALL
EXTRA_LINKFLAGS +=-Wl,--no-as-needed -Wl,--whole-archive ../../../lib/libstoreinst.a -Wl,--no-whole-archive

MAPS_OBJS := ../btree/btree.o

# Change CC and CXX before including common to automatically configure
# PUDDLES_CXX and PUDDLES_CC
export CC=../../storeinst-pass/dclang
export CXX=../../storeinst-pass/dclang++
export DCLANG_DONT_LINK=1

include ../../common.make

all: map

map: $(OBJECTS)
	$(PUDDLES_CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(MAPS_OBJS) $(LINKFLAGS)

clean: clean_deps
	-rm -f *.o map

include ../../deps.make
