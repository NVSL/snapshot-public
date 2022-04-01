// -*- mode: c++; c-basic-offset: 2; -*-

#include "libc_wrappers.hh"
#include "libstoreinst.hh"
#include "log.hh"
#include "nvsl/common.hh"
#include "nvsl/pmemops.hh"
#include "nvsl/string.hh"
#include "nvsl/utils.hh"
#include "recovery.hh"
#include "utils.hh"

#include <cassert>
#include <dlfcn.h>
#include <filesystem>
#include <numeric>
#include <sys/mman.h>
#include <unistd.h>

namespace fs = std::filesystem;
using namespace nvsl;

/*-- LIBC functions BEGIN --*/
typedef void *(*memcpy_sign)(void *, const void *, size_t);
typedef void *(*mmap_sign)(void *, size_t, int, int, int, __off_t);
typedef void *(*mremap_sign)(void *__addr, size_t __old_len, size_t __new_len,
                             int __flags, ...);
typedef int (*munmap_sign)(void *__addr, size_t __len);
typedef void (*sync_sign)(void);
typedef int (*fsync_sign)(int);
typedef int (*msync_sign)(void *addr, size_t length, int flags);

void *(*real_memcpy)(void *, const void *, size_t) = nullptr;
void *(*real_memmove)(void *, const void *, size_t) = nullptr;
void *(*real_mmap)(void *__addr, size_t __len, int __prot, int __flags, int __fd,
                   __off_t __offset) = nullptr;
void *(*real_mremap)(void *__addr, size_t __old_len, size_t __new_len,
                     int __flags, ...) = nullptr;
int (*real_munmap)(void *__addr, size_t __len) = nullptr;
void (*real_sync)(void) = nullptr;
int (*real_fsync)(int) = nullptr;
int (*real_fdatasync)(int) = nullptr;
int (*real_msync)(void *addr, size_t length, int flags);
/*-- LIBC functions END --*/

/** @brief Map from file descriptor to mapped address range **/
std::unordered_map<int, cxlbuf::addr_range_t> cxlbuf::mapped_addr;

/** @brief Bump allocator for the START_ADDR -> END_ADDR mmap tracking space **/
void *cxlbuf::mmap_start = nullptr;

void cxlbuf::init_dlsyms() {
  real_memcpy = (memcpy_sign)dlsym(RTLD_NEXT, "memcpy");
  printf("dlsym for memcpy = %p at %p\n", (void*)real_memcpy, (void*)&real_memcpy);
  if (real_memcpy == nullptr) {
    DBGE << "dlsym failed for memcpy: %s\n" << std::string(dlerror()) << "\n";
    exit(1);
  }

  real_memmove = (memcpy_sign)dlsym(RTLD_NEXT, "memmove");
  if (real_memmove == nullptr) {
    DBGE << "dlsym failed for memmove: %s\n" << std::string(dlerror()) << "\n";
    exit(1);
  }

  real_mmap = (mmap_sign)dlsym(RTLD_NEXT, "mmap");
  if (real_mmap == nullptr) {
    DBGE << "dlsym failed for mmap: %s\n" << std::string(dlerror()) << "\n";
    exit(1);
  }

  real_sync = (sync_sign)dlsym(RTLD_NEXT, "sync");
  if (real_sync == nullptr) {
    DBGE << "dlsym failed for sync: %s\n" << std::string(dlerror()) << "\n";
    exit(1);
  }

  real_fsync = (fsync_sign)dlsym(RTLD_NEXT, "fsync");
  if (real_fsync == nullptr) {
    DBGE << "dlsym failed for fsync: %s\n" << std::string(dlerror()) << "\n";
    exit(1);
  }

  real_fdatasync = (fsync_sign)dlsym(RTLD_NEXT, "fdatasync");
  if (real_fdatasync == nullptr) {
    DBGE << "dlsym failed for fdatasync: %s\n" << std::string(dlerror()) << "\n";
    exit(1);
  }

  real_mremap = (mremap_sign)dlsym(RTLD_NEXT, "mremap");
  if (real_mremap == nullptr) {
    DBGE << "dlsym failed for mremap: %s\n" << std::string(dlerror()) << "\n";
    exit(1);
  }

  real_msync = (msync_sign)dlsym(RTLD_NEXT, "msync");
  if (real_msync == nullptr) {
    DBGE << "dlsym failed for msync: %s\n" << std::string(dlerror()) << "\n";
    exit(1);
  }

  real_munmap = (munmap_sign)dlsym(RTLD_NEXT, "munmap");
  if (real_munmap == nullptr) {
    DBGE << "dlsym failed for munmap: %s\n" << std::string(dlerror()) << "\n";
    exit(1);
  }
}

extern "C" {
void *memcpy(void *__restrict dst, const void *__restrict src, size_t n) __THROW {
  if (startTracking and start_addr != nullptr and addr_in_range(dst)) {
    tls_log.log_range(dst, n);
  }

  return real_memcpy(dst, src, n);
}

void *memmove(void *__restrict dst, const void *__restrict src, size_t n)
  __THROW {
  bool memmove_logged = false;
  if (startTracking and start_addr != nullptr and addr_in_range(dst)) {
    tls_log.log_range(dst, n);
    memmove_logged = true;
  }
  
  DBGH(4) << "memmove(" << dst << ", " << src << ", " << n << "), logged = "
          << memmove_logged << std::endl;

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

  DBGH(3) << "Mmaped fd " << __fd << " to path " << S(buf) << std::endl;

  if (cxlbuf::mmap_start == nullptr) {
    cxlbuf::mmap_start = start_addr;
    assert(cxlbuf::mmap_start != nullptr);
  }

  cxlbuf::PmemFile pmemf(S(buf), __addr, __len);

  // Check if the mmaped file is in /mnt/pmem0/
  if (is_prefix("/mnt/pmem0/", buf)) {
    DBGH(1) << "Changing mmap address from " << __addr << " to "
            << cxlbuf::mmap_start << std::endl;
    __addr = cxlbuf::mmap_start;

    /* mmap needs aligned address */
    cxlbuf::mmap_start = (char*)cxlbuf::mmap_start + ((__len+4095)/4096)*4096;


    pmemf.set_addr(__addr);
    pmemf.create_backing_file();
    pmemf.map_backing_file();
  }  

  DBGH(1) << "Call to mmap intercepted: "
          << mmap_to_str(__addr, __len, __prot, __flags, __fd, __offset)
          << std::endl;

  void *result = pmemf.map_to_page_cache(__flags, __prot, __fd, __offset);
  
  return result;
}
  
void *mmap64(void *addr, size_t len, int prot, int flags, int fd, __off64_t off)
  __THROW {
  return mmap(addr, len, prot, flags, fd, off);
} 

void sync(void) __THROW {
  DBGE << "Call to sync intercepted\n";
  DBGE << "Unimplemented\n";
  exit(1);

  real_sync();
}

int msync(void *__addr, size_t __len, int __flags) {
  return snapshot(__addr, __len, __flags);
}


int fsync(int __fd) __THROW {
  DBGE << "Call to fsync intercepted: fsync(" << __fd << ")";
  DBGE << "Unimplemented\n";
  exit(1);

  return real_fsync(__fd);
}

void *mremap (void *__addr, size_t __old_len, size_t __new_len,
              int __flags, ...) __THROW {
  DBGE << "Unimplemented\n";
  exit(1);
  return real_mremap(__addr, __old_len, __new_len, __flags);
}

  

int fdatasync (int __fildes) {
  int result = 0;
  const bool addr_mapped
    = cxlbuf::mapped_addr.find(__fildes) != cxlbuf::mapped_addr.end();
  cxlbuf::addr_range_t range = {};
  if (addr_mapped) {
    range = cxlbuf::mapped_addr[__fildes];
  }

  DBGH(3) << "Call to fdatasync intercepted: fdatasync(" << __fildes << "=["
          << (void*)range.start << ":" << (void*)range.end << "])\n";
  if (addr_mapped and addr_in_range((void*)(range.start+1))){
    DBGH(2) << "Calling snapshot with (" << (void*)range.start << ", "
            << range.end - range.start << ", " << MAP_SYNC << ")\n";
    
    result = snapshot((void*)range.start, range.end - range.start, MS_SYNC);
    DBGH(3) << "Snapshot returned " << result << "\n";
  } else {
    result = real_fdatasync(__fildes);
  }
  
  return result;
}

__attribute__((unused))
int snapshot(void *addr, size_t bytes, int flags) {
  char *pm_back = (char*)0x20000000000;

  /* Drain all the stores to the log before modifying the backing file */
  pmemops->drain();
  
  DBGH(1) << "Call to snapshot(" << addr << ", " << bytes << ", " << flags
          << ")\n";
  if (storeInstEnabled) [[likely]] {
    DBGH(1) << "Calling snapshot (not msync)" << std::endl;

    size_t total_proc = 0;
    size_t bytes_flushed = 0;
    size_t start = (uint64_t)addr, end = (uint64_t)addr+bytes;
    size_t diff = end - start;

    for (auto &entry : tls_log.entries) {
      /* Copy the logged location to the backing store if in range of
         snapshot */
      if (entry.addr - start <= diff) [[likely]] {
        const size_t offset = entry.addr - (uint64_t)start_addr;
        const size_t dst_addr = (size_t)(pm_back + offset);
        
        DBGH(4) << "Copying " << entry.bytes << " bytes from "
                << (void*)entry.addr << " -> " << (void*)dst_addr
                << std::endl;

        if (entry.bytes > 64) {
          real_memcpy((void*)dst_addr, (void*)entry.addr, entry.bytes);
          pmemops->flush((void*)dst_addr, entry.bytes);
        } else {
          pmemops->streaming_wr((void*)dst_addr, (void*)entry.addr,
                                entry.bytes);
        }

        entry.addr = 0;
      } else if (entry.addr == 0) {
        break;
      }

      bytes_flushed += entry.bytes;
      total_proc++;
    }

    DBGH(4) << "total_proc = " << total_proc << "\n";
    DBGH(4) << "bytes_flushed = " << bytes_flushed << "\n";
    
    cxlbuf::tx_log_count_dist->add(total_proc);

    pmemops->drain();
      
  } else {
    DBGH(1) << "Calling real msync" << std::endl;
    
    void *pg_aligned = (void*)(((size_t)addr>>12)<<12);

    const int mret = real_msync(pg_aligned, bytes, flags);

    if (-1 == mret) {
      DBGE << "msync(" << pg_aligned << ", " << bytes << ", " << flags << ")\n";
      perror("msync for snapshot failed");
      exit(1);
    }
  }
  tls_log.current_log_off = 0;
  tls_log.entries.clear();
  return 0;
}

int munmap (void *__addr, size_t __len) __THROW {
  DBGH(4) << "mumap intercepted\n";
  for (auto &range : cxlbuf::mapped_addr) {
    if (__addr >= (void*)range.second.start 
        && __addr < (void*)range.second.end) {
      if (__addr != (void*)range.second.start) {
        DBGE << "Unmapping part of address range not supported" << std::endl;
        exit(1);
      } else {
        cxlbuf::mapped_addr.erase(cxlbuf::mapped_addr.find(range.first));
        break;
      }
    }
  }

  return real_munmap(__addr, __len);
}
}
