// -*- mode: c++; c-basic-offset: 2; -*-

#include "libc_wrappers.hh"
#include "libstoreinst.hh"
#include "log.hh"
#include "nvsl/common.hh"
#include "nvsl/string.hh"

#include <cassert>
#include <dlfcn.h>
#include <numeric>
#include <sys/mman.h>

typedef void *(*memcpy_sign)(void *, const void *, size_t);
typedef void *(*mmap_sign)(void*, size_t, int, int, int, __off_t);

void *(*real_memcpy)(void *, const void *, size_t) = nullptr;
void *(*real_memmove)(void *, const void *, size_t) = nullptr;
void *(*real_mmap)(void *, size_t, int, int, int, __off_t) = nullptr;

void *mmap_start = nullptr;

void init_dlsyms() {
  real_memcpy = (memcpy_sign)dlsym(RTLD_NEXT, "memcpy");
  if (real_memcpy == nullptr) {
    std::cerr << "dlsym failed for memcpy: " << dlerror() << std::endl;
    exit(1);
  }

  real_memmove = (memcpy_sign)dlsym(RTLD_NEXT, "memmove");
  if (real_memmove == nullptr) {
    std::cerr << "dlsym failed for memmove: " << dlerror() << std::endl;
    exit(1);
  }

  real_mmap = (mmap_sign)dlsym(RTLD_NEXT, "mmap");
  if (real_mmap == nullptr) {
    std::cerr << "dlsym failed for mmap: " << dlerror() << std::endl;
    exit(1);
  }
}

extern "C" {
void *memcpy(void *__restrict dst, const void *__restrict src, size_t n) __THROW {
  if (startTracking and start_addr != nullptr and start_addr <= dst and
      dst < end_addr) {
    log_range(dst, n);
  }

  return real_memcpy(dst, src, n);
}

void *memmove(void *__restrict dst, const void *__restrict src, size_t n)
  __THROW {
  if (startTracking and start_addr != nullptr and start_addr <= dst and
      dst < end_addr) {
    log_range(dst, n);
  }

  return real_memmove(dst, src, n);
}

void *mmap(void *__addr, size_t __len, int __prot, int __flags, int __fd,
            __off_t __offset) __THROW {
  /* Read the file name for the fd */
  auto fd_path = "/proc/self/fd/" + std::to_string(__fd);
  constexpr size_t path_max = 4096;
  char buf[path_max];
  if (-1 == readlink(fd_path.c_str(), buf, path_max)) {
    perror("readlink for mmap failed");
    exit(1);
  }

  printf("mmaped fd %d to path %s\n", __fd, buf);

  if (mmap_start == nullptr) {
    mmap_start = start_addr;
    assert(mmap_start != nullptr);
  }

  // Check if the mmaped file is in /mnt/pmem0/
  if (nvsl::is_prefix("/mnt/pmem0/", buf)) {
    printf("Changing mmap address from %p to %p\n", __addr, mmap_start);
    __addr = mmap_start;
    mmap_start = (char*)mmap_start + __len;
  }

  std::vector<std::string> flags;
  if (__flags & MAP_SHARED) {
    flags.push_back("MAP_SHARED");
  }
  if (__flags & MAP_PRIVATE) {
    flags.push_back("MAP_PRIVATE");
  }
  if (__flags & MAP_ANONYMOUS) {
    flags.push_back("MAP_ANONYMOUS");
  }
  if (__flags & MAP_FIXED) {
    flags.push_back("MAP_FIXED");
  }
  if (__flags & MAP_SYNC) {
    flags.push_back("MAP_SYNC");
  }

  auto flags_str = nvsl::zip(flags, " | ");

  std::vector<std::string> prot;
  if (__prot & PROT_READ) {
    prot.push_back("PROT_READ");
  }
  if (__prot & PROT_WRITE) {
    prot.push_back("PROT_WRITE");
  }
  if (__prot & PROT_EXEC) {
    prot.push_back("PROT_EXEC");
  }

  auto prot_str = nvsl::zip(prot, " | ");

  printf("Call to mmap intercepted: mmap(%p, %lu, %s, %s, %d, %ld)\n", __addr,
         __len, prot_str.c_str(), flags_str.c_str(), __fd, __offset);

  return real_mmap(__addr, __len, __prot, __flags, __fd, __offset);
}
}
