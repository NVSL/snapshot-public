all: bin lib | check_compiler
	$(PUDDLES_MAKE) -C src

release:
	$(PUDDLES_MAKE) all PUDDLES_CXXFLAGS="-DRELEASE" RELEASE=1

clean: clean_src
	@-rm -rf bin/ lib/ 
	@-rm -f .so_deps

clean_src:
	$(PUDDLES_MAKE) -C src clean PERFORM_CHECKS=0

bin:
	@mkdir -p bin/
	@mkdir -p bin/examples/

lib:
	@mkdir -p lib/

.PHONY: all

include src/common.make
include src/checks.make
include src/features.make
