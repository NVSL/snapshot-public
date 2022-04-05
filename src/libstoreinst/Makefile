SOURCES := $(wildcard *.cc)

OBJECTS := $(patsubst %.cc, %.o, $(SOURCES))

LIB_DIR :=../../lib
LIB_DIR := $(shell readlink -f $(CURDIR)/$(LIB_DIR))

INCLUDE +=-iquote./

DEPENDS := $(wildcard ../include/* ./*.hh ./*/*.hh)

EXTRA_LDFLAGS := -L$(LIB_DIR) -Wl,-R$(LIB_DIR) -Wl,--no-as-needed
EXTRA_LINKFLAGS +=-Wl,--whole-archive $(ARCHIVE_DEPENDS) \
	-Wl,--no-whole-archive
EXTRA_LINKFLAGS +=-luuid -pthread
TARGET := libstoreinst

include ../common.make

.PHONY: all
all: $(TARGET).a $(TARGET).so

$(TARGET).so: $(OBJECTS) $(DEPENDS) $(ARCHIVE_DEPENDS)
	$(PUDDLES_CXX) $(CXXFLAGS) $(INCLUDE) $(LDFLAGS) -shared \
		-o $@ $(OBJECTS) $(LINKFLAGS)

$(TARGET).a: $(OBJECTS)
	$(PUDDLES_AR) rcs $@ $^


clean: 
	-rm -f *.o *.so *.a

include ../deps.make
