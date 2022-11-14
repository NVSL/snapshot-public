.PHONY: libdsaemu libpmbuffer libstoreinst examples storeinst-pass libvram libvramfs libcxlfs libpmdkemul

include common.make

all: libdsaemu libpmbuffer examples

dclang_links:
	$(PUDDLES_MAKE) ../bin/dclang
	$(PUDDLES_MAKE) ../bin/dclang++

examples: libstoreinst libpmdkemul
	$(PUDDLES_MAKE) -C examples

libdsaemu:
	$(PUDDLES_MAKE) -C libdsaemu
	$(PUDDLES_MAKE) ../lib/libdsaemu.so

libpmdkemul: storeinst-pass dclang_links
	$(PUDDLES_MAKE) -C libpmdkemul
	$(PUDDLES_MAKE) ../lib/libpmdkemul.so

libstoreinst: libvram libvramfs libcxlfs
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

libvramfs:
	$(PUDDLES_MAKE) -C libvramfs
	$(PUDDLES_MAKE) ../lib/libvramfs.so

libcxlfs:
	$(PUDDLES_MAKE) -C libcxlfs
	$(PUDDLES_MAKE) ../lib/libcxlfs.so

../lib/%:
	$(PUDDLES_LN) -fs $(basename $(patsubst ../lib/%, ../src/%, $@))/$(notdir $@) $@

../bin/dclang ../bin/dclang++:
	$(PUDDLES_LN) -fs $(basename $(patsubst ../bin/%, ../src/storeinst-pass/%, $@)) $@

clean: libpmbuffer_clean libdsaemu_clean examples_clean storeinst-pass_clean \
	scripts_clean libstoreinst_clean libpmdkemul_clean
	rm "$(SELF_DIR).make.tmp"

%_clean:
	$(PUDDLES_MAKE) -C $(subst _clean,,$@) clean
