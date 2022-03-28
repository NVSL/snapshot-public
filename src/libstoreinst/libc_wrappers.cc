// -*- mode: c++; c-basic-offset: 2; -*-

#include "libc_wrappers.hh"
#include "libstoreinst.hh"
#include "log.hh"
#include "nvsl/common.hh"
#include "nvsl/string.hh"
#include "utils.hh"

#include <cassert>
#include <dlfcn.h>
#include <numeric>
#include <sys/mman.h>
#include <unistd.h>
#include <unordered_map>

typedef void *(*memcpy_sign)(void *, const void *, size_t);
typedef void *(*mmap_sign)(void *, size_t, int, int, int, __off_t);
typedef void *(*mremap_sign)(void *__addr, size_t __old_len, size_t __new_len,
                             int __flags, ...);
typedef void (*sync_sign)(void);
typedef int (*fsync_sign)(int);

void *(*real_memcpy)(void *, const void *, size_t) = nullptr;
void *(*real_memmove)(void *, const void *, size_t) = nullptr;
void *(*real_mmap)(void *, size_t, int, int, int, __off_t) = nullptr;
void *(*real_mremap)(void *__addr, size_t __old_len, size_t __new_len,
                     int __flags, ...) = nullptr;
void (*real_sync)(void) = nullptr;
int (*real_fsync)(int) = nullptr;
int (*real_fdatasync)(int) = nullptr;

/* Map from file descriptor to mapped address range */
struct addr_range_t {
  size_t start;
  size_t end;
};
std::unordered_map<int, addr_range_t> mapped_addr;

void *mmap_start = nullptr;

void init_dlsyms() {
  real_memcpy = (memcpy_sign)dlsym(RTLD_NEXT, "memcpy");
  if (real_memcpy == nullptr) {
    fprintf(stderr, "dlsym failed for memcpy: %s\n", dlerror());
    exit(1);
  }

  real_memmove = (memcpy_sign)dlsym(RTLD_NEXT, "memmove");
  if (real_memmove == nullptr) {
    fprintf(stderr, "dlsym failed for memmove: %s\n", dlerror());
    exit(1);
  }

  real_mmap = (mmap_sign)dlsym(RTLD_NEXT, "mmap");
  if (real_mmap == nullptr) {
    fprintf(stderr, "dlsym failed for mmap: %s\n", dlerror());
    exit(1);
  }

  real_sync = (sync_sign)dlsym(RTLD_NEXT, "sync");
  if (real_sync == nullptr) {
    fprintf(stderr, "dlsym failed for sync: %s\n", dlerror());
    exit(1);
  }

  real_fsync = (fsync_sign)dlsym(RTLD_NEXT, "fsync");
  if (real_fsync == nullptr) {
    fprintf(stderr, "dlsym failed for fsync: %s\n", dlerror());
    exit(1);
  }

  real_fdatasync = (fsync_sign)dlsym(RTLD_NEXT, "fdatasync");
  if (real_fdatasync == nullptr) {
    fprintf(stderr, "dlsym failed for fdatasync: %s\n", dlerror());
    exit(1);
  }

  real_mremap = (mremap_sign)dlsym(RTLD_NEXT, "mremap");
  if (real_mremap == nullptr) {
    fprintf(stderr, "dlsym failed for mremap: %s\n", dlerror());
    exit(1);
  }
}

extern "C" {
void *memcpy(void *__restrict dst, const void *__restrict src, size_t n) __THROW {
  if (startTracking and start_addr != nullptr and addr_in_range(dst)) {
    log_range(dst, n);
  }

  return real_memcpy(dst, src, n);
}

void *memmove(void *__restrict dst, const void *__restrict src, size_t n)
  __THROW {
  if (startTracking and start_addr != nullptr and addr_in_range(dst)) {
    log_range(dst, n);
  }

  return real_memmove(dst, src, n);
}

void *mmap64(void *addr, size_t len, int prot, int flags, int fd, __off64_t off)
  __THROW {
  return mmap(addr, len, prot, flags, fd, off);
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

  fprintf(stderr, "mmaped fd %d to path %s\n", __fd, buf);

  if (mmap_start == nullptr) {
    mmap_start = start_addr;
    assert(mmap_start != nullptr);
  }

  // Check if the mmaped file is in /mnt/pmem0/
  if (nvsl::is_prefix("/mnt/pmem0/", buf)) {
    fprintf(stderr, "Changing mmap address from %p to %p\n", __addr, mmap_start);
    __addr = mmap_start;
    mmap_start = (char*)mmap_start + ((__len+4095)/4096)*4096;
  }

  /* Add map fixed no replace to prevent kernel from mapping it outside the
     tracking space */
  __flags |= MAP_FIXED_NOREPLACE;
  
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

  const auto flags_str = nvsl::zip(flags, " | ");

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

  const auto prot_str = nvsl::zip(prot, " | ");


  fprintf(stderr, "Call to mmap intercepted: mmap(%p, %lu, %s, %s, %d (=%s), %ld)\n",
          __addr, __len, prot_str.c_str(), flags_str.c_str(), __fd, buf, __offset);

  void *result = real_mmap(__addr, __len, __prot, __flags, __fd, __offset);

  if (result != nullptr) {
    const addr_range_t val = {(size_t)(__addr), (size_t)((char*)__addr + __len)};
    mapped_addr.insert({__fd, val});

    if (not addr_in_range(result)) {
      DBGE << "Cannot map pmem file in range" << std::endl;
      system(("cat /proc/" + std::to_string(getpid()) + "/maps >&2").c_str());
      exit(1);
    }
  } else {
    DBGW << "Call to mmap failed" << std::endl;
  }

  return result;
}

void sync(void) __THROW {
  fprintf(stderr, "Call to sync intercepted\n");
  fprintf(stderr, "Unimplemented\n");
  exit(1);

  real_sync();
}

int fsync(int __fd) __THROW {
  fprintf(stderr, "Call to fsync intercepted: fsync(%d)\n", __fd);
  fprintf(stderr, "Unimplemented\n");
  exit(1);

  return real_fsync(__fd);
}

void *mremap (void *__addr, size_t __old_len, size_t __new_len,
              int __flags, ...) __THROW {
  fprintf(stderr, "Unimplemented\n");
  exit(1);
  return real_mremap(__addr, __old_len, __new_len, __flags);
}

  

int fdatasync (int __fildes) {
  int result = 0;
  const bool addr_mapped = mapped_addr.find(__fildes) != mapped_addr.end();
  const auto range = mapped_addr[__fildes];
  
  fprintf(stderr, "Call to fdatasync intercepted: fdatasync(%d)\n", __fildes);
  if (addr_mapped and addr_in_range((void*)(range.start+1))){
    fprintf(stderr, "Calling snapshot with (%p, %lu, %d)\n", (void*)range.start,
            range.end - range.start, MAP_SYNC);
    result = snapshot((void*)range.start, range.end - range.start, MS_SYNC);
    fprintf(stderr, "Snapshot returned %d\n", result);    
  } else {
    result = real_fdatasync(__fildes);
  }
  
  return result;
}

}
