.PHONY: libdsaemu libpmbuffer libstoreinst examples storeinst-pass libvram

include common.make

all: libdsaemu libpmbuffer examples dclang_links

dclang_links:
	$(PUDDLES_MAKE) ../bin/dclang
	$(PUDDLES_MAKE) ../bin/dclang++

examples: libstoreinst storeinst-pass
	$(PUDDLES_MAKE) -C examples

libdsaemu:
	$(PUDDLES_MAKE) -C libdsaemu
	$(PUDDLES_MAKE) ../lib/libdsaemu.so

libstoreinst:
	$(PUDDLES_MAKE) -C libstoreinst
	$(PUDDLES_MAKE) ../lib/libstoreinst.so
	$(PUDDLES_MAKE) ../lib/libstoreinst.a

storeinst-pass:
	$(PUDDLES_MAKE) -C storeinst-pass
	$(PUDDLES_LN) -fs ../src/storeinst-pass/build/libstoreinstpass.so ../lib/libstoreinstpass.so

libpmbuffer:
	$(PUDDLES_MAKE) -C libpmbuffer
	$(PUDDLES_MAKE) ../lib/libpmbuffer.so

libvram:
	$(PUDDLES_MAKE) -C libvram
	$(PUDDLES_MAKE) ../lib/libvram.so

../lib/%:
	$(PUDDLES_LN) -fs $(basename $(patsubst ../lib/%, ../src/%, $@))/$(notdir $@) $@

../bin/dclang ../bin/dclang++:
	$(PUDDLES_LN) -fs $(basename $(patsubst ../bin/%, ../src/storeinst-pass/%, $@)) $@

clean: libpmbuffer_clean libdsaemu_clean examples_clean storeinst-pass_clean \
	scripts_clean libstoreinst_clean

%_clean:
	$(PUDDLES_MAKE) -C $(subst _clean,,$@) clean
