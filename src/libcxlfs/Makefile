SOURCES := $(wildcard *.cc)

OBJECTS := $(patsubst %.cc, %.o, $(SOURCES))

LIB_DIR :=../../lib
LIB_DIR := $(shell readlink -f $(CURDIR)/$(LIB_DIR))

INCLUDE +=-I./ $(shell PKG_CONFIG_PATH="$(CXLBUF_PKG_CONFIG_PATH)" pkg-config --cflags spdk_nvme spdk_env_dpdk)

DEPENDS := $(wildcard ../include/* ./*.hh ./*/*.hh)


EXTRA_LDFLAGS := -L$(LIB_DIR) -Wl,-R$(LIB_DIR) -Wl,--no-as-needed
EXTRA_LINKFLAGS +=-Wl,--whole-archive $(ARCHIVE_DEPENDS) \
	-Wl,--no-whole-archive
EXTRA_LINKFLAGS += $(shell PKG_CONFIG_PATH=$(CXLBUF_PKG_CONFIG_PATH) pkg-config --libs spdk_nvme spdk_env_dpdk libisal spdk_vmd)
EXTRA_LINKFLAGS += -Wl,-R$(ROOT_DIR)/vendor/spdk/build/lib -Wl,-R$(ROOT_DIR)/vendor/spdk/dpdk/build-tmp/lib
TARGET := libcxlfs.so

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
