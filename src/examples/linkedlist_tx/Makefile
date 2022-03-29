SOURCES := $(wildcard *.cc)
OBJECTS := $(patsubst %.cc, %.o, $(SOURCES))
DEPENDS := $(wildcard ../../include/*)
# DEPENDS += $(wildcard ./*.hh)
INCLUDE :=-iquote../../include
# INCLUDE +=-iquote../../

DIR      := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))
LIBS_DIR := $(shell readlink -f $(DIR)../../../lib)
EXTRA_LDFLAGS +=-L$(LIBS_DIR) -Wl,-R$(LIBS_DIR) -Wl,--no-as-needed
EXTRA_LINKFLAGS +=-lrt -luuid -lpthread

# Change CC and CXX before including common to automatically configure
# PUDDLES_CXX and PUDDLES_CC
export CC=../../storeinst-pass/dclang
export CXX=../../storeinst-pass/dclang++

include ../../common.make

all: linkedlist_tx 

linkedlist_tx: linkedlist_tx.o
	$(PUDDLES_CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LINKFLAGS)

clean: clean_deps
	-rm -f *.o linkedlist_tx ../map/map.o

include ../../deps.make
