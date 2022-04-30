DEPS_DIR := .deps

# Disable symbol visibility=hidden for non-release builds
ifdef RELEASE
VISBILITY_HIDDEN := -fvisibility=hidden
endif

%.o: %.cc $(DEPS_DIR)/%.d | $(DEPS_DIR)
	$(PUDDLES_CXX) $(INCLUDE) -mclwb -mavx -mavx2 -c $(CXXFLAGS)\
		$(PUDDLES_CXXFLAGS) $< -MT $@ -o $@ -MMD -MP -MF $(DEPS_DIR)/$*.d

$(DEPS_DIR): ; @mkdir -p $@

DEP_FILES := $(SOURCES:%.cc=$(DEPS_DIR)/%.d)
$(DEP_FILES):

include $(wildcard $(DEP_FILES))

clean_deps:
	-rm -rf ./.deps
