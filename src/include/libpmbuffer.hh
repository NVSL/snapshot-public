// -*- mode: c++; c-basic-offset: 2; -*-

#pragma once

/**
 * @file   libpmbuffer.hh
 * @date   février  4, 2022
 * @brief  Brief description here
 */

#include <cstddef>
#include <stdint.h>

namespace pmbuffer {
  struct pmbuf_t {
    char *raw;
    size_t bytes;
  };

  extern pmbuf_t* get_pmbuffer();
}
