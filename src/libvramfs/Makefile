SOURCES := $(wildcard *.cc)

OBJECTS := $(patsubst %.cc, %.o, $(SOURCES))

LIB_DIR :=../../lib
LIB_DIR := $(shell readlink -f $(CURDIR)/$(LIB_DIR))

INCLUDE +=-I./

DEPENDS := $(wildcard ../include/* ./*.hh ./*/*.hh)

EXTRA_LDFLAGS := -L$(LIB_DIR) -Wl,-R$(LIB_DIR) -Wl,--no-as-needed
EXTRA_LINKFLAGS +=-Wl,--whole-archive $(ARCHIVE_DEPENDS) \
	-Wl,--no-whole-archive
EXTRA_LINKFLAGS +=-luuid -lvulkan

TARGET := libvramfs.so

include ../common.make

all:
	$(PUDDLES_MAKE) library

library: $(TARGET)

$(TARGET): $(OBJECTS) $(DEPENDS) $(ARCHIVE_DEPENDS)
	$(PUDDLES_CXX) $(CXXFLAGS) $(INCLUDE) $(LDFLAGS) -shared \
			-o $@ $(OBJECTS) $(LINKFLAGS)

clean: 
	-rm -f *.o *.so *.a

include ../deps.make
