#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <sys/mman.h>
#include <vector>

#include "libcxlfs/libcxlfs.hh"
#include "nvsl/common.hh"
#include "nvsl/utils.hh"

using namespace nvsl;
using namespace nvsl::libcxlfs;

typedef std::string cxlfs_path_t;
typedef int cxlfs_fd_t;

constexpr size_t DEF_ALLOC_SIZE = 4UL * 1024 * 1024 * 1024;

struct fd_desc_t {
  int fd;             //< file descriptor identifier
  void *region;       //< Start of the region where this file is allocated
  size_t len;         //< Current file length
  off_t va_alloc_off; //< Location (offset) in bytes from the start of the
                      // allocation region
  off_t seek;         //< Current seek

  /** @brief Get the end pointer of this file in its region */
  void *end() const {
    auto result = RCast<char *>(region) + va_alloc_off + seek;
    return RCast<void *>(result);
  }

  /** @brief Get the offset where this file ends in its region */
  size_t end_off() const {
    auto result = va_alloc_off + seek;
    return result;
  }
};

std::unordered_map<cxlfs_path_t, cxlfs_fd_t> path_to_fd;
std::unordered_map<cxlfs_fd_t, cxlfs_path_t> fd_to_path;
std::unordered_map<cxlfs_fd_t, fd_desc_t> fd_desc;

int get_free_fd() {
  int result = 1;
  while (fd_to_path.find(result) != fd_to_path.end()) {
    result++;
  }

  return result;
}

bool fd_exists(const int fd) {
  return (fd_desc.find(fd) != fd_desc.end());
}

int create_fd(const std::string &pathname_str) {
  int fd = get_free_fd();

  fd_to_path[fd] = pathname_str;
  path_to_fd[pathname_str] = fd;

  fd_desc[fd] = {};

  fd_desc[fd].fd = fd;
  fd_desc[fd].region = 0;
  fd_desc[fd].len = 0;
  fd_desc[fd].va_alloc_off = 0;
  fd_desc[fd].seek = 0;

  DBGH(3) << "Created a new file descriptor with:\n"
          << "\t.fd = " << fd_desc[fd].fd << "\n"
          << "\t.region = " << fd_desc[fd].region << "\n"
          << "\t.len = " << fd_desc[fd].len << "\n"
          << "\t.va_alloc_off = " << fd_desc[fd].va_alloc_off << "\n"
          << "\t.seek = " << fd_desc[fd].seek << "\n";

  return fd;
}

off_t nvsl::libcxlfs::lseek(int fd, off_t offset, int whence) {
  off_t new_off = 0;

  if (not fd_exists(fd)) {
    errno = EBADF;
    return (off_t)-1;
  }

  auto &fd_desc_e = fd_desc[fd];

  new_off = fd_desc_e.seek;

  if (whence == SEEK_SET) {
    new_off = offset;
  } else {
    errno = ENOSYS;
    return (off_t)-1;
  }

  fd_desc_e.seek = new_off;

  NVSL_ASSERT(fd_desc_e.len == 0,
              "SEEK_SET is currently only supported on a new fd");

  if ((size_t)new_off > fd_desc_e.len) {
    fd_desc_e.len = new_off;
    fd_desc_e.region = nvsl::libvram::malloc(fd_desc_e.len);
  }

  return new_off;
}

int nvsl::libcxlfs::open(const char *pathname, int flags) {
  const auto pathname_str = std::string(pathname);
  int result = -1;

  if (path_to_fd.find(pathname) != path_to_fd.end()) {
    result = path_to_fd[pathname_str];
  } else {
    result = create_fd(pathname_str);
  }

  return result;
}

void *nvsl::libcxlfs::mmap(void *addr, size_t length, int prot, int flags,
                            int fd, off_t offset) {
  void *result;

  if (not fd_exists(fd)) {
    errno = EBADF;
    return MAP_FAILED;
  }

  if ((flags & MAP_FIXED) or (flags & MAP_FIXED_NOREPLACE)) {
    errno = ENOTSUP;
    return MAP_FAILED;
  }

  if ((flags & MAP_PRIVATE) or (flags & MAP_ANONYMOUS)) {
    errno = ENOTSUP;
    return MAP_FAILED;
  }

  auto &fd_desc_e = fd_desc[fd];

  if (length + offset > fd_desc_e.len) {
    DBGW << "length + offset (=" << length + offset
         << ") is greater than file size " << fd_desc_e.len << "\n";
    errno = EINVAL;
    return MAP_FAILED;
  }

  result = RCast<char *>(fd_desc_e.region) + fd_desc_e.va_alloc_off;

  return result;
}
