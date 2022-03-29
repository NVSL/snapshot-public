// -*- mode: c++; c-basic-offset: 2; -*-

#include "libc_wrappers.hh"
#include "libstoreinst.hh"
#include "log.hh"
#include "nvsl/common.hh"
#include "nvsl/pmemops.hh"
#include "nvsl/string.hh"
#include "utils.hh"

#include <cassert>
#include <dlfcn.h>
#include <filesystem>
#include <numeric>
#include <sys/mman.h>
#include <unistd.h>
#include <unordered_map>

namespace fs = std::filesystem;

using namespace nvsl;

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
    log_range(dst, n);
  }

  return real_memcpy(dst, src, n);
}

void *memmove(void *__restrict dst, const void *__restrict src, size_t n)
  __THROW {
  bool memmove_logged = false;
  if (startTracking and start_addr != nullptr and addr_in_range(dst)) {
    log_range(dst, n);
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

  if (mmap_start == nullptr) {
    mmap_start = start_addr;
    assert(mmap_start != nullptr);
  }

  // Check if the mmaped file is in /mnt/pmem0/
  if (is_prefix("/mnt/pmem0/", buf)) {
    DBGH(1) << "Changing mmap address from " << __addr << " to " << mmap_start
            << std::endl;
    __addr = mmap_start;

    /* mmap needs aligned address */
    mmap_start = (char*)mmap_start + ((__len+4095)/4096)*4096;

    /* Since this file is opened in the PM, create a corresponding backing file
       and map it at +0x10000000000 */
    const fs::path bck_fname = S(buf) + S(".cxlbuf_backing");

    if (fs::exists(bck_fname)) {
      fs::remove_all(bck_fname);
    }
    
    fs::copy_file(S(buf), bck_fname);

    void *bck_addr = (char*)__addr + 0x10000000000;
    const int bck_fd = open(bck_fname.c_str(), O_RDWR);
    const int bck_flags = MAP_SHARED_VALIDATE | MAP_SYNC | MAP_FIXED_NOREPLACE;
    const void *mbck_addr = real_mmap(bck_addr, __len, bck_flags,
                                      PROT_READ | PROT_WRITE, bck_fd, 0);

    if (mbck_addr == (void*)-1) {
      DBGE << "Unable to map backing file" << std::endl;
      exit(1);
    } else {
      DBGH(2) << "Backing file " << bck_fname << " for " << fs::path(buf)
              << " mapped at address " << mbck_addr << std::endl;
    }
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
  if (__flags & MAP_FIXED_NOREPLACE) {
    flags.push_back("MAP_FIXED_NOREPLACE");
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


  DBGH(3) << "Call to mmap intercepted: mmap(" << __addr << ", " << __len
          << ", " << prot_str << ", " << flags_str << ", " << __fd << ", "
          << nvsl::S(buf) << ", " << __offset << std::endl;

  void *result = real_mmap(__addr, __len, __prot, __flags, __fd, __offset);

  if (result != nullptr) {
    const addr_range_t val = {(size_t)(__addr), (size_t)((char*)__addr + __len)};
    mapped_addr.insert_or_assign(__fd, val);
    DBGH(2) << "mmap_addr recorded " << __fd << " -> " << (void*)val.start
            << (void*)val.end << std::endl;

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
  const bool addr_mapped = mapped_addr.find(__fildes) != mapped_addr.end();
  addr_range_t range = {};
  if (addr_mapped) {
    range = mapped_addr[__fildes];
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
  
  DBGH(1) << "Call to snapshot(" << addr << ", " << bytes << ", " << flags
          << ")\n";
  if (storeInstEnabled) {
    DBGH(1) << "Calling snapshot (not msync)" << std::endl;

    auto log = (log_t*)log_area;
    size_t total_proc = 0;
    size_t bytes_flushed = 0;
    for (size_t i = 0; i < current_log_off;) {
      auto &log_entry = *(log_t*)((char*)log + i);

      /* Copy the logged location to the backing store if in range of
         snapshot */
      if (log_entry.addr < (uint64_t)addr + bytes
          and log_entry.addr >= (uint64_t)addr) {

        const size_t offset = log_entry.addr - (uint64_t)start_addr;
        const size_t dst_addr = (size_t)(pm_back + offset);
        
        DBGH(4) << "Copying " << log_entry.bytes << " bytes from "
                << (void*)log_entry.addr << " -> " << (void*)dst_addr
                << std::endl;

        real_memcpy((void*)dst_addr, (void*)log_entry.addr, log_entry.bytes);
        pmemops->flush((void*)dst_addr, log_entry.bytes);

        log_entry.addr = 0;
      } else if (log_entry.addr == 0) {
        break;
      }

      bytes_flushed += log_entry.bytes;
      total_proc++;

      i += sizeof(log_t) + log_entry.bytes;
    }

    DBGH(4) << "total_proc = " << total_proc << "\n";
    DBGH(4) << "bytes_flushed = " << bytes_flushed << "\n";
    
    tx_log_count_dist->add(total_proc);

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
  current_log_off = 0;
  return 0;
}

int munmap (void *__addr, size_t __len) __THROW {
  DBGH(4) << "mumap intercepted\n";
  for (auto &range : mapped_addr) {
    if (__addr >= (void*)range.second.start 
        && __addr < (void*)range.second.end) {
      if (__addr != (void*)range.second.start) {
        DBGE << "Unmapping part of address range not supported" << std::endl;
        exit(1);
      } else {
        mapped_addr.erase(mapped_addr.find(range.first));
        break;
      }
    }
  }

  return real_munmap(__addr, __len);
}
}
