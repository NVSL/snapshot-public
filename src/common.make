SELF_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
DEPS_DIR := .deps
ROOT_DIR := $(realpath $(dir $(dir $(realpath $(lastword $(MAKEFILE_LIST)))))..)/

include $(SELF_DIR)vars.make
include $(SELF_DIR)colors.make

CONFIG_FILE ?= $(SELF_DIR)../make.config

ifdef RELEASE
CXXFLAGS    :=-std=gnu++20 -ggdb3 -O3 -g3 -march=native -fPIC -Wall -DNDEBUG
CXXFLAGS    +=-fomit-frame-pointer
else
CXXFLAGS    :=-std=gnu++20 -ggdb3 -O0 -fPIC -Wall
CXXFLAGS    += -fno-omit-frame-pointer -Werror
endif

ifdef DEBUG_BUILD
CXXFLAGS    += -v
endif


# Format the make.config file and import it if it's out of date
$(shell \
if [ ! -f "$(SELF_DIR).make.tmp" ]; then \
	cat $(CONFIG_FILE) | grep -v '^#' | egrep '(=y)' | sed 's|=y||g' | sed 's|^|CXXFLAGS += -D|g' > $(SELF_DIR)/.make.tmp;\
fi;\
)
$(shell \
if [ "$(CONFIG_FILE)" -nt "$(SELF_DIR).make.tmp" ]; then \
	cat $(CONFIG_FILE) | grep -v '^#' | egrep '(=y)' | sed 's|=y||g' | sed 's|^|CXXFLAGS += -D|g' > $(SELF_DIR)/.make.tmp;\
fi;\
)
include $(SELF_DIR).make.tmp


CXXFLAGS    += -Wpedantic
CXXFLAGS    += $(EXTRA_CXXFLAGS) $(PUDDLE_CXXFLAGS)
CXXFLAGS    +=$(CLWB_FLAG) $(CLFLUSHOPT_FLAG) $(SFENCE_FLAG)
LDFLAGS     :=$(EXTRA_LDFLAGS)
LINKFLAGS   :=$(EXTRA_LINKFLAGS)
INCLUDE     +=-iquote$(SELF_DIR)include
INCLUDE     +=-iquote$(SELF_DIR)
INCLUDE     +=-iquote$(SELF_DIR)../vendor/cpp-common/include
INCLUDE     +=-I$(BOOST_INTERPROCESS_PATH)
INCLUDE     +=-I$(BOOST_MOVE_PATH)
INCLUDE     +=-I$(BOOST_CONFIG_PATH)

ifdef RELEASE
export RELEASE=1
CXXFLAGS    +=-DRELEASE
endif

PUDDLES_CC      =$(QUIET_CC)$(CC)
PUDDLES_OPT     =$(QUIET_OPT)$(OPT)
PUDDLES_CXX     =$(QUIET_CXX)$(CXX)
PUDDLES_DXX     =$(QUIET_DXX)$(DXX)
PUDDLES_LN      =$(QUIET_LINK)ln
PUDDLES_MAKE    =+$(QUIET_MAKE)
PUDDLES_AR      =+$(QUIET_AR)$(AR)
PUDDLES_INSTALL =+$(QUIET_INSTALL)install

ifneq (,$(findstring dclang,$(CC)))
  CC_NAME := DC
else
  CC_NAME := CC
endif

ifneq (,$(findstring dclang++,$(CXX)))
  CXX_NAME := DXX
else
  CXX_NAME := CXX
endif

ifndef V
  QUIET_OPT     = @printf '    %b %b\n' $(CCCOLOR)OPT$(ENDCOLOR) $(SRCCOLOR)$@$(ENDCOLOR) 1>&2;
  QUIET_CC      = @printf '     %b %b\n' $(CCCOLOR)$(CC_NAME)$(ENDCOLOR) $(SRCCOLOR)$@$(ENDCOLOR) 1>&2;
  QUIET_CXX     = @printf '    %b %b\n'  $(CCCOLOR)$(CXX_NAME)$(ENDCOLOR) $(SRCCOLOR)$@$(ENDCOLOR) 1>&2;
  QUIET_DXX     = @printf '    %b %b\n'  $(CCCOLOR)DXX$(ENDCOLOR) $(SRCCOLOR)$@$(ENDCOLOR) 1>&2;
  QUIET_LINK    = @printf '   %b %b\n' $(LINKCOLOR)LINK$(ENDCOLOR) $(BINCOLOR)$@$(ENDCOLOR) 1>&2;
  QUIET_MAKE    = @printf '   %b %b\n'   $(MAKECOLOR)MAKE$(ENDCOLOR) $(SRCCOLOR)$@$(ENDCOLOR) 1>&2;$(MAKE) -s
  QUIET_AR      = @printf '     %b %b\n'   $(ARCOLOR)AR$(ENDCOLOR) $(SRCCOLOR)$@$(ENDCOLOR) 1>&2;
  QUIET_INSTALL = @printf '%b %b\n'   $(INSTALLCOLOR)INSTALL$(ENDCOLOR) $(SRCCOLOR)$@$(ENDCOLOR) 1>&2;
else
  QUIET_MAKE    = $(MAKE)
endif

