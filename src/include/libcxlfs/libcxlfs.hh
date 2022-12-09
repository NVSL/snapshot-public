#pragma once

#include "nvsl/constants.hh"
#include <cstddef>

class Controller;

namespace nvsl {
  namespace libcxlfs {
    extern Controller *ctrlr;

    constexpr size_t MEM_SIZE = 16UL * nvsl::LP_SZ::GiB;
    constexpr size_t INIT_CACHE_SIZE = 128 * nvsl::LP_SZ::MiB;
    constexpr size_t CACHE_SIZE = 128 * nvsl::LP_SZ::MiB;

    void flush_caches();

    void *malloc(size_t bytes);

    /**
     * @brief Open a file on vram
     * @param[in] pathname Path of the file to open
     * @param flags Currently ignored
     * @return A file descriptor
     */
    int open(const char *pathname, int flags);

    /**
     * @brief Close an existing vram_fd
     */
    void *close(int fd);

    /**
     * @brief MMAP a vram fd
     */
    void *mmap(void *addr, size_t length, int prot, int flags, int fd,
               off_t offset);

    /**
     * @brief seek a vramfd
     * @warn Only SEEK_SET supported
     */
    off_t lseek(int fd, off_t offset, int whence);
  } // namespace libcxlfs
} // namespace nvsl
