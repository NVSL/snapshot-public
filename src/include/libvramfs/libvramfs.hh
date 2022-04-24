#pragma once

namespace nvsl {
  namespace libvramfs {
    /**
     * @brief Open a file on vram
     * @param[in] pathname Path of the file to open
     * @param flags Currently ignored
     * @return A file descriptor
     */
    int open(const char *pathname, int flags);

    /**
     * @brief Close and existing vram_fd
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
  } // namespace libvramfs
} // namespace nvsl
