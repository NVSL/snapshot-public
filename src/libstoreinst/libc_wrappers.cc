// -*- mode: c++; c-basic-offset: 2; -*-

#include "libc_wrappers.hh"
#include "libstoreinst.hh"
#include "log.hh"
#include "nvsl/clock.hh"
#include "nvsl/common.hh"
#include "nvsl/envvars.hh"
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
typedef void *(*memset_sign)(void *s, int c, size_t n);
typedef void *(*mmap_sign)(void *, size_t, int, int, int, __off_t);
typedef void *(*mremap_sign)(void *__addr, size_t __old_len, size_t __new_len,
                             int __flags, ...);
typedef int (*munmap_sign)(void *__addr, size_t __len);
typedef void (*sync_sign)(void);
typedef int (*fsync_sign)(int);
typedef int (*msync_sign)(void *addr, size_t length, int flags);

void *(*real_memcpy)(void *, const void *, size_t) = nullptr;
void *(*real_memmove)(void *, const void *, size_t) = nullptr;
void *(*real_memset)(void *s, int c, size_t n) = nullptr;
void *(*real_mmap)(void *__addr, size_t __len, int __prot, int __flags,
                   int __fd, __off_t __offset) = nullptr;
void *(*real_mremap)(void *__addr, size_t __old_len, size_t __new_len,
                     int __flags, ...) = nullptr;
int (*real_munmap)(void *__addr, size_t __len) = nullptr;
void (*real_sync)(void) = nullptr;
int (*real_fsync)(int) = nullptr;
int (*real_fdatasync)(int) = nullptr;
int (*real_msync)(void *addr, size_t length, int flags);
/*-- LIBC functions END --*/

/** @brief Map from file descriptor to mapped address range **/
std::unordered_map<int, cxlbuf::fd_metadata_t> cxlbuf::mapped_addr;

/** @brief Bump allocator for the START_ADDR -> END_ADDR mmap tracking space **/
void *cxlbuf::mmap_start = nullptr;

nvsl::Clock *perst_overhead_clk;
nvsl::Counter snapshots;

void cxlbuf::init_dlsyms() {
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

  real_memset = (memset_sign)dlsym(RTLD_NEXT, "memset");
  if (real_memset == nullptr) {
    DBGE << "dlsym failed for memset: %s\n" << std::string(dlerror()) << "\n";
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
    DBGE << "dlsym failed for fdatasync: %s\n"
         << std::string(dlerror()) << "\n";
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
void *memcpy(void *__restrict dst, const void *__restrict src,
             size_t n) __THROW {
  assert(real_memcpy != nullptr);

  if (startTracking and start_addr != nullptr and addr_in_range(dst)) {
    tls_log.log_range(dst, n);
  }

  return real_memcpy(dst, src, n);
}

void *memmove(void *__restrict dst, const void *__restrict src,
              size_t n) __THROW {
  bool memmove_logged = false;

  assert(real_memmove != nullptr);

  if (startTracking and start_addr != nullptr and addr_in_range(dst)) {
    tls_log.log_range(dst, n);
    memmove_logged = true;
  }

  DBGH(4) << "memmove(" << dst << ", " << src << ", " << n
          << "), logged = " << memmove_logged << std::endl;

  return real_memmove(dst, src, n);
}

void *memset(void *s, int c, size_t n) {
//  fprintf(stderr, "Memset [%p:%p] %lu KiB\n", s, (void *)((char *)s + n),
//          n / 1024);
  return real_memset(s, c, n);
}

void *mmap(void *__addr, size_t __len, int __prot, int __flags, int __fd,
           __off_t __offset) __THROW {
  /* Read the file name for the fd */
  const auto fd_fname = nvsl::fd_to_fname(__fd);

  cxlbuf::PmemFile pmemf(fd_fname, __addr, __len);

  DBGH(1) << "Call to mmap intercepted (for " << fs::path(fd_fname) << "): "
          << mmap_to_str(__addr, __len, __prot, __flags, __fd, __offset)
          << std::endl;

  // Check if the mmaped file is in /mnt/pmem0/
  if (is_prefix("/mnt/pmem0/", fd_fname) or is_prefix("/mnt/cxl0", fd_fname)) {
    if (cxlbuf::mmap_start == nullptr) {
      cxlbuf::mmap_start = start_addr;
    }

    DBGH(1) << "Changing mmap address from " << __addr << " to "
            << cxlbuf::mmap_start << std::endl;
    __addr = cxlbuf::mmap_start;

    /* mmap needs aligned address */
    cxlbuf::mmap_start =
        (char *)cxlbuf::mmap_start + ((__len + 4095) / 4096) * 4096;

    pmemf.set_addr(__addr);
    pmemf.create_backing_file();
    pmemf.map_backing_file();
  }

  void *result = pmemf.map_to_page_cache(__flags, __prot, __fd, __offset);

  return result;
}

void *mmap64(void *addr, size_t len, int prot, int flags, int fd,
             __off64_t off) __THROW {
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
  DBGH(1) << "Call to fsync intercepted: fsync(" << __fd << ")";

  using nvsl::cxlbuf::mapped_addr;

  if (mapped_addr.find(__fd) == mapped_addr.end()) {
    DBGE << "File descriptor " << __fd << " not found in the mapped addresses"
         << std::endl;
    exit(1);
  }

  const auto fd_metadata = mapped_addr[__fd];

  if (storeInstEnabled) {
    return snapshot((void *)fd_metadata.range.start,
                    fd_metadata.range.end - fd_metadata.range.start, MS_SYNC);
  } else {
    return real_fsync(__fd);
  }
}

void *mremap(void *__addr, size_t __old_len, size_t __new_len, int __flags,
             ...) __THROW {
  DBGE << "Unimplemented\n";
  exit(1);
  return real_mremap(__addr, __old_len, __new_len, __flags);
}

int fdatasync(int __fildes) {
  int result = 0;
  const bool addr_mapped =
      cxlbuf::mapped_addr.find(__fildes) != cxlbuf::mapped_addr.end();
  cxlbuf::addr_range_t range = {};
  if (addr_mapped) {
    range = cxlbuf::mapped_addr[__fildes].range;
  }

  DBGH(1) << "Call to fdatasync intercepted: fdatasync(" << __fildes << "=["
          << (void *)range.start << ":" << (void *)range.end << "])\n";
  if (addr_mapped and addr_in_range((void *)(range.start + 1))) {
    DBGH(2) << "Calling snapshot with (" << (void *)range.start << ", "
            << range.end - range.start << ", " << MAP_SYNC << ")\n";

    result = snapshot((void *)range.start, range.end - range.start, MS_SYNC);
    DBGH(3) << "Snapshot returned " << result << "\n";
  } else {
    result = real_fdatasync(__fildes);
  }

  return result;
}

__attribute__((unused)) int snapshot(void *addr, size_t bytes, int flags) {
  snapshots++;
  if (nopMsync) [[unlikely]] {
    return 0;
  }

#ifdef CXLBUF_TESTING_GOODIES
  perst_overhead_clk->tick();
#endif

#ifdef TRACE_LOG_MSYNC
  const std::string snapshot_msg = "snapshot\n";
  write(trace_fd, snapshot_msg.c_str(), strlen(snapshot_msg.c_str()));
#endif

  auto pm_back = RCast<uint8_t *>(cxlbuf::backing_file_start);

  tls_log.flush_all();

  /* Drain all the stores to the log and update its state before modifying the
     backing file */
  tls_log.set_state(cxlbuf::Log::State::ACTIVE, true);

  DBGH(1) << "Call to snapshot(" << addr << ", " << bytes << ", " << flags
          << ")\n";
  if (storeInstEnabled) [[likely]] {
    if (firstSnapshot) [[unlikely]] {
      DBGH(1) << "== First snapshot ==" << std::endl;
      firstSnapshot = false;

      /* If this is the first snapshot, copy all the mapped regions from their
         source to the backing files. This allows us to get the two copies on
         parity. */
      for (const auto &entry : cxlbuf::mapped_addr) {
        const auto fname = entry.second.fpath;
        const auto erange = entry.second.range;

        if (erange.start <= (size_t)addr and erange.end > (size_t)addr) {
          DBGH(2) << "Found the entry " << entry.first
                  << " (=" << fs::path(fname) << ") [" << (void *)erange.start
                  << ", " << (void *)erange.end << "]" << std::endl;

          const size_t off = erange.start - 0x10000000000;
          void *dst = RCast<uint8_t *>(nvsl::cxlbuf::backing_file_start) + off;

          const void *src = (void *)(erange.start);

          const size_t memcpy_sz =
              fname == "" ? (erange.end - erange.start) : fs::file_size(fname);

          /* Copy all the allocated bytes from the actual file to the backing */
          DBGH(4) << "Calling real_memcpy(" << dst << ", " << src << ", "
                  << memcpy_sz << ")" << std::endl;
          real_memcpy(dst, src, memcpy_sz);
          break;
        }
      }
    }

    DBGH(1) << "Calling snapshot (not msync)" << std::endl;

    size_t total_proc = 0;
    size_t bytes_flushed = 0;
    size_t start = (uint64_t)addr, end = (uint64_t)addr + bytes;
    size_t diff = end - start;

#if LOG_FORMAT_VOLATILE
    const auto &log_list = tls_log.entries;
#elif LOG_FORMAT_NON_VOLATILE
    const auto &log_list = *tls_log.log_area;
#else
#error "Log format needs to be volatile or non-volatile."
#endif

    for (const auto &entry : log_list) {
      /* Copy the logged location to the backing store if in range of
         snapshot */
      if (entry.addr - start <= diff) [[likely]] {
        const size_t offset = entry.addr - (uint64_t)start_addr;
        const size_t dst_addr = (size_t)(pm_back + offset);

        DBGH(4) << "Copying " << entry.bytes << " bytes from "
                << (void *)(0UL + entry.addr) << " -> " << (void *)dst_addr
                << std::endl;

        /* Streaming write is only allowed if the write size is a power of two
           (popcount == 1) and dest address is aligned at entry.bytes */
        const bool str_wr_allowed =
            (std::popcount(entry.bytes) == 1) and (dst_addr % entry.bytes == 0);

        DBGH(4) << "Streaming write allowed? " << str_wr_allowed << "\n";

        if (str_wr_allowed and entry.bytes >= 8) [[likely]] {
          pmemops->streaming_wr((void *)dst_addr, (void *)(0UL + entry.addr),
                                entry.bytes);
        } else {
          real_memcpy((void *)dst_addr, (void *)(0UL + entry.addr),
                      entry.bytes);
          pmemops->flush((void *)dst_addr, entry.bytes);
        }
      } else if (entry.addr == 0) {
        break;
      }

      bytes_flushed += entry.bytes;
      total_proc++;
    }

    DBGH(4) << "total_proc = " << total_proc << "\n";
    DBGH(4) << "bytes_flushed = " << bytes_flushed << "\n";

#ifndef RELEASE
    cxlbuf::tx_log_count_dist->add(total_proc);
#endif

#ifdef CXLBUF_TESTING_GOODIES
    if (crashOnCommit) [[unlikely]] {
      DBGW << "Crashing before commit (CXLBUF_CRASH_ON_COMMIT is set)"
           << std::endl;
      exit(1);
    }
#endif // CXLBUF_TESTING_GOODIES

    /* Update the state to drop the log and drain all the updates to the backing
       file */
    pmemops->drain();
    tls_log.set_state(cxlbuf::Log::State::EMPTY);
  } else {
    DBGH(1) << "Calling real msync" << std::endl;

    void *pg_aligned = (void *)(((size_t)addr >> 12) << 12);

    const int mret = real_msync(pg_aligned, bytes, flags);

    if (-1 == mret) {
      DBGE << "msync(" << pg_aligned << ", " << bytes << ", " << flags << ")\n";
      perror("msync for snapshot failed");
      exit(1);
    }
  }

  tls_log.clear();

#ifdef CXLBUF_TESTING_GOODIES
  perst_overhead_clk->tock();
#endif // CXLBUF_TESTING_GOODIES

  return 0;
}

int munmap(void *__addr, size_t __len) __THROW {
  DBGH(4) << "mumap intercepted\n";
  for (auto &range : cxlbuf::mapped_addr) {
    if (__addr >= (void *)range.second.range.start &&
        __addr < (void *)range.second.range.end) {
      if (__addr != (void *)range.second.range.start) {
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
