ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

.PHONY: all
all: bin lib | check_compiler
	$(PUDDLES_MAKE) -C src
#	$(PUDDLES_MAKE) vendor

.PHONY: vendor
vendor:
	$(PUDDLES_MAKE) -C vendor DD=$(ROOT_DIR)/bin/dclang \
		DXX=$(ROOT_DIR)/bin/dclang++ \
		DCLANG_LIBS_DIR="$(ROOT_DIR)/lib"

release:
	$(PUDDLES_MAKE) all PUDDLES_CXXFLAGS="-DRELEASE" RELEASE=1

.PHONY: clean
clean: clean_src clean_vendor
	@-rm -rf bin/ lib/
	@-rm -f .so_deps

.PHONY: clean_src
clean_src:
	$(PUDDLES_MAKE) -C src clean PERFORM_CHECKS=0

.PHONY: clean_vendor
clean_vendor:
	$(PUDDLES_MAKE) -C vendor clean

bin:
	@mkdir -p bin/
	@mkdir -p bin/examples/

lib:
	@mkdir -p lib/

.PHONY: tests
tests:
	$(PUDDLES_MAKE) -C tests/
	LD_LIBRARY_PATH=lib/:$(LD_LIBRARY_PATH) tests/test.bin  --gmock_verbose=info --gtest_stack_trace_depth=10
	src/scripts/tests/run.sh


include src/common.make
include src/checks.make
include src/features.make
