SOURCES := $(wildcard *.cc)
OBJECTS := $(patsubst %.cc, %.o, $(SOURCES))
DEPENDS := $(wildcard ../../include/*)
INCLUDE :=-iquote../../include

DIR      := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))
LIBS_DIR := $(shell readlink -f $(DIR)../../../lib)
EXTRA_LDFLAGS +=-lpthread -L$(LIBS_DIR) -Wl,-R$(LIBS_DIR)

MAPS_OBJS := ../btree/btree.o

include ../../common.make

all: map

map: $(OBJECTS)
	$(PUDDLES_CXX) $(CXXFLAGS) $^ $(MAPS_OBJS) -o $@ $(LDFLAGS)

clean: clean_deps
	-rm -f *.o map

include ../../deps.make
