.PHONY: libdsaemu libpmbuffer examples

include common.make

all: libdsaemu libpmbuffer examples

examples:
	$(PUDDLES_MAKE) -C examples

libdsaemu:
	$(PUDDLES_MAKE) -C libdsaemu
	$(PUDDLES_MAKE) ../lib/libdsaemu.so

libpmbuffer:
	$(PUDDLES_MAKE) -C libpmbuffer
	$(PUDDLES_MAKE) ../lib/libpmbuffer.so

../lib/%:
	$(PUDDLES_LN) -fs $(basename $(patsubst ../lib/%, ../src/%, $@))/$(notdir $@) $@

clean: libdsaemu_clean

%_clean:
	$(PUDDLES_MAKE) -C $(subst _clean,,$@) clean
