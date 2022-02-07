// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   libdsaemu.hh
 * @date   février  4, 2022
 * @brief  Brief description here
 */

#include <cstddef>
#include <stdint.h>

namespace dsa {
  typedef uint64_t addr_t;
  const uint64_t QSIZE = 1024;

  struct jobdesc_t {
    addr_t src;
    addr_t dst;
    size_t bytes;
  };

  extern size_t queue_head, queue_tail;
  extern jobdesc_t queue[QSIZE];

  __attribute__((constructor)) void libdsaemu_ctor();
  void clear_queue();
}
