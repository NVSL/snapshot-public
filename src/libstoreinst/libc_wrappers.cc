// -*- mode: c++; c-basic-offset: 2; -*-

#include "libc_wrappers.hh"
#include "libstoreinst.hh"
#include "log.hh"
#include "nvsl/common.hh"

#include <dlfcn.h>
#include <numeric>

typedef void *(*memcpy_sign)(void *, const void *, size_t);

void *(*real_memcpy)(void *, const void *, size_t) = nullptr;
void *(*real_memmove)(void *, const void *, size_t) = nullptr;

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
}

extern "C" {
void *memcpy(void *__restrict dst, const void *__restrict src, size_t n) __THROW {
  if (startTracking and start_addr != nullptr and start_addr <= dst and
      dst < end_addr) {
    log_range(dst, n);
  }

  return real_memcpy(dst, src, n);
}

void *memmove(void *__restrict dst, const void *__restrict src, size_t n) __THROW {
  if (startTracking and start_addr != nullptr and start_addr <= dst and
      dst < end_addr) {
    log_range(dst, n);
  }

  return real_memmove(dst, src, n);
}
}
