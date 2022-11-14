ifeq (,$(wildcard $(ROOT_DIR)vendor/interprocess/include))
  ifndef BOOST_INTERPROCESS_PATH
    $(error Missing: $(ROOT_DIR)vendor/interprocess/include. Did you clone recursively? To use a different version, set BOOST_INTERPROCESS_PATH to the root of your version.)
  endif
endif

BOOST_INTERPROCESS_PATH ?= $(ROOT_DIR)vendor/interprocess/include

ifeq (,$(wildcard $(ROOT_DIR)vendor/move/include))
  ifndef BOOST_MOVE_PATH
    $(error Missing: $(ROOT_DIR)vendor/move/include. Did you clone recursively? To use a different version, set BOOST_MOVE_PATH to the root of your version.)
  endif
endif

BOOST_MOVE_PATH ?= $(ROOT_DIR)vendor/move/include

ifeq (,$(wildcard $(ROOT_DIR)vendor/config/include))
  ifndef BOOST_CONFIG_PATH
    $(error Missing: $(ROOT_DIR)vendor/config/include. Did you clone recursively? To use a different version, set BOOST_CONFIG_PATH to the root of your version.)
  endif
endif

BOOST_CONFIG_PATH ?= $(ROOT_DIR)vendor/config/include

CXLBUF_PKG_CONFIG_PATH="$(PKG_CONFIG_PATH):$(ROOT_DIR)vendor/spdk/build/lib/pkgconfig"
