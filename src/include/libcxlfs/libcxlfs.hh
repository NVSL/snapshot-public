#pragma once

#include <cstddef>

namespace nvsl {
  namespace libcxlfs {
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
