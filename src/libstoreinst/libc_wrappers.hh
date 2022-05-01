// -*- mode: c++; c-basic-offset: 2; -*-

#pragma once

#include <cstdint>
#include <filesystem>
#include <unistd.h>
#include <unordered_map>

extern void *(*real_memcpy)(void *, const void *, size_t);
extern void *(*real_memmove)(void *, const void *, size_t);
extern void *(*real_mmap)(void *__addr, size_t __len, int __prot, int __flags,
                          int __fd, __off_t __offset);
extern void *(*real_mremap)(void *__addr, size_t __old_len, size_t __new_len,
                            int __flags, ...);
extern int (*real_munmap)(void *__addr, size_t __len);
extern void (*real_sync)(void);
extern int (*real_fsync)(int);
extern int (*real_fdatasync)(int);
extern int (*real_msync)(void *addr, size_t length, int flags);

namespace nvsl {
  namespace cxlbuf {
    struct addr_range_t {
      size_t start;
      size_t end;
    };

    /** @brief Struct to track information about mapped pmem regions */
    struct fd_metadata_t {
      int fd;
      cxlbuf::addr_range_t range;
      std::filesystem::path fpath;
    };

    /** @brief Map from file descriptor to mapped address range **/
    extern std::unordered_map<int, fd_metadata_t> mapped_addr;

    /** @brief Bump allocator for the START_ADDR -> END_ADDR mmap tracking space
     * **/
    extern void *mmap_start;

    void init_dlsyms();
  } // namespace cxlbuf
} // namespace nvsl
