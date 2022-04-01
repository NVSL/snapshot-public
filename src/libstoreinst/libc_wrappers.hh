// -*- mode: c++; c-basic-offset: 2; -*-

#pragma once

#include <cstdint>
#include <unistd.h>
#include <unordered_map>

extern void *(*real_memcpy)(void *, const void *, size_t);
extern void *(*real_memmove)(void *, const void *, size_t);
extern void *(*real_mmap)(void *, size_t, int, int, int, __off_t);

namespace nvsl {
  namespace cxlbuf {
    struct addr_range_t {
      size_t start;
      size_t end;
    };

    /** @brief Map from file descriptor to mapped address range **/
    extern std::unordered_map<int, addr_range_t> mapped_addr;

    /** @brief Bump allocator for the START_ADDR -> END_ADDR mmap tracking space **/
    extern void *mmap_start;

    void init_dlsyms();
  }
}
