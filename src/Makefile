.PHONY: libdsaemu examples

include common.make

all: libdsaemu examples

examples:
	$(PUDDLES_MAKE) -C examples

libdsaemu:
	$(PUDDLES_MAKE) -C libdsaemu
	$(PUDDLES_MAKE) ../lib/libdsaemu.so

../lib/%:
	$(PUDDLES_LN) -fs $(basename $(patsubst ../lib/%, ../src/%, $@))/$(notdir $@) $@

clean: libdsaemu_clean

%_clean:
	$(PUDDLES_MAKE) -C $(subst _clean,,$@) clean
