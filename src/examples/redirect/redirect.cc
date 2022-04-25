// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   redirect.cc
 * @date   avril 25, 2022
 * @brief  Brief description here
 */

#include <dlfcn.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "libvram/libvram.hh"
#include "nvsl/error.hh"
#include "nvsl/utils.hh"

typedef int (*munmap_sign)(void *__addr, size_t __len);
typedef void *(*mmap_sign)(void *, size_t, int, int, int, __off_t);

int (*real_munmap)(void *__addr, size_t __len) = nullptr;
void *(*real_mmap)(void *__addr, size_t __len, int __prot, int __flags,
                   int __fd, __off_t __offset) = nullptr;

/** @brief Initialize all the dlsyms (mmap and munmap) */
void init_dlsyms() {
  real_mmap = (mmap_sign)dlsym(RTLD_NEXT, "mmap");
  if (real_mmap == nullptr) {
    DBGE << "dlsym failed for mmap: %s\n" << std::string(dlerror()) << "\n";
    exit(1);
  }

  real_munmap = (munmap_sign)dlsym(RTLD_NEXT, "munmap");
  if (real_munmap == nullptr) {
    DBGE << "dlsym failed for munmap: %s\n" << std::string(dlerror()) << "\n";
    exit(1);
  }
}

std::unordered_map<std::string, void *> mapped_files;

void *mmap(void *__addr, size_t __len, int __prot, int __flags, int __fd,
           __off_t __offset) __THROW {
  const std::string fname = nvsl::fd_to_fname(__fd);
  void *result = MAP_FAILED;

  if (nvsl::is_prefix("/mnt/cxl0/", fname)) {
    DBGW << "MMAP intercepted for redirect (fd = " << fname << "): "
         << nvsl::mmap_to_str(__addr, __len, __prot, __flags, __fd, __offset)
         << "\n";
    DBGW << "Calling nvsl::libvram::malloc(" << (void *)__len << ")\n";

    if (mapped_files.find(fname) != mapped_files.end()) {
      result = mapped_files[fname];
    } else {
      result = nvsl::libvram::malloc(__len);
      mapped_files[fname] = result;
    }
  } else {
    result = real_mmap(__addr, __len, __prot, __flags, __fd, __offset);
  }

  return result;
}

int munmap(void *__addr, size_t __len) __THROW {
  return 0;
}

/**
 * @brief Constructor for the redirect shared object
 */
__attribute__((__constructor__(101))) void redirect_ctor() {
  init_dlsyms();
  ctor_libvram();
}
