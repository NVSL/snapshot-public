// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   reservoir.hh
 * @date   mars 14, 2022
 * @brief  Brief description here
 */

#pragma once

#include <cassert>
#include <unistd.h>
#include <iostream>

namespace libpuddles {
  class reservoir_t {
  private:
    char *region;
    size_t sz_bytes;
    size_t bump_offset = 0;
  public:
    reservoir_t(void *region, size_t bytes) : region((char*)region),
                                              sz_bytes(bytes), bump_offset(0) {
    }
    
    template <typename T>
    T *malloc(const size_t count = 1) {
      assert(bump_offset < sz_bytes);
      
      T *result = (T*)(region + bump_offset);

      bump_offset += sizeof(T) * count;

      // std::cout << "Malloc bump now at " << bump_offset << std::endl;
      
      return result;
    };

    void free(void *ptr) {
      
    }
  };
}
