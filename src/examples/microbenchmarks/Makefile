SOURCES := $(wildcard *.cc)
OBJECTS := $(patsubst %.cc, %.o, $(SOURCES))
DEPENDS := $(wildcard ../../include/*)
INCLUDE :=-iquote../../include

DIR      := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))
LIBS_DIR := $(shell readlink -f $(DIR)../../../lib)
EXTRA_LDFLAGS +=-L$(LIBS_DIR) -Wl,-R$(LIBS_DIR) -Wl,--no-as-needed

include ../../common.make

all: run

run: $(OBJECTS)
	$(PUDDLES_CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LINKFLAGS)

clean: clean_deps
	-rm -f *.o run

include ../../deps.make
