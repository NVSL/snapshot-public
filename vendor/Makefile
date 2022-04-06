ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

.PHONY: all
all:
	$(PUDDLES_MAKE) -C kyoto
	$(PUDDLES_MAKE) -C kyoto.inst CC=$(ROOT_DIR)/bin/dclang CXX=$(ROOT_DIR)/bin/dclang++

.PHONY: clean
clean:
	$(PUDDLES_MAKE) -C kyoto clean
	$(PUDDLES_MAKE) -C kyoto.inst clean

include ../src/common.make
