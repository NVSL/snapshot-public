ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

.PHONY: all
all: kyoto_dir

redis_deps:
	$(PUDDLES_MAKE) -C redis/deps geohash-int hiredis jemalloc linenoise lua
	$(PUDDLES_MAKE) -C redis/deps/pmdk

redis.inst_deps:
	$(PUDDLES_MAKE) -C redis.inst/deps geohash-int hiredis jemalloc linenoise lua

%_dir: %_normal %_inst
	@echo "lkadf"

kyoto_deps kyoto.inst_deps:

%_normal: %_deps
	$(PUDDLES_MAKE) -C $(patsubst %_normal,%,$@) DISABLE_PASS=1 DCLANG_DONT_LINK=1 CC=$(ROOT_DIR)/bin/dclang CXX=$(ROOT_DIR)/bin/dclang++ USE_PMDK=yes STD=-std=gnu99

%_inst: %.inst_deps
	$(PUDDLES_MAKE) -C $(patsubst %_inst,%.inst,$@) USE_PMDK=yes STD=-std=gnu99 CC=$(ROOT_DIR)/bin/dclang CXX=$(ROOT_DIR)/bin/dclang++

.PHONY: clean

clean: redis_clean kyoto_clean

redis_clean:
	$(PUDDLES_MAKE) -C $(patsubst %_clean,%,$@) distclean
	$(PUDDLES_MAKE) -C $(patsubst %_clean,%.inst,$@) distclean

%_clean:
	$(PUDDLES_MAKE) -C $(patsubst %_clean,%,$@) clean
	$(PUDDLES_MAKE) -C $(patsubst %_clean,%.inst,$@) clean

include ../src/common.make
