#include "common.hh"
#include <cassert>
#include <dlfcn.h>
#include <fcntl.h>
#include <immintrin.h>
#include <iostream>
#include <numeric>
#include <sys/mman.h>

#include "nvsl/common.hh"
#include "nvsl/envvars.hh"
#include "nvsl/stats.hh"

#define PMEM_START_ADDR_ENV "PMEM_START_ADDR"
#define PMEM_END_ADDR_ENV "PMEM_END_ADDR"
#define BUF_SIZE (100 * 1000 * 4096)

static nvsl::Counter *skip_check_count, *logged_check_count;
static nvsl::StatsFreq<> *tx_log_count_dist;

extern "C" {
  static void *start_addr, *end_addr = nullptr;
  static char *log_area, *pm_back;
  static size_t current_log_off = 0;
  bool startTracking = false;
  static bool storeInstEnabled = false;
  static bool cxlModeEnabled = false;
  
#define MEMCPY_SIGN (void *(*)(void *, const void *, size_t))
  typedef void* (*memcpy_sign)(void*,const void*,size_t);
  static void* (*real_memcpy)(void*,const void*,size_t) = nullptr;
  static void* (*real_memmove)(void*,const void*,size_t) = nullptr;

  struct log_t {
    uint64_t addr;
    uint64_t bytes;

    NVSL_BEGIN_IGNORE_WPEDANTIC
    uint64_t val[];
    NVSL_END_IGNORE_WPEDANTIC
  };

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

  void init_counters() {
    skip_check_count = new nvsl::Counter();
    logged_check_count = new nvsl::Counter();
    tx_log_count_dist = new nvsl::StatsFreq<>();

    skip_check_count->init("skip_check_count", "Skipped memory checks");
    logged_check_count->init("logged_check_count", "Logged memory checks");
    tx_log_count_dist->init("tx_log_count_dist", "Distribution of number of logs in a transaction", 5, 0, 30);
  }

  void init_addrs() {
    const auto start_addr_str = get_env_str(PMEM_START_ADDR_ENV);
    if (start_addr_str == "") {
      std::cerr << "PMEM_START_ADDR not set" << std::endl;
      exit(1);
    }

    const auto end_addr_str = get_env_str(PMEM_END_ADDR_ENV);
    if (end_addr_str == "") {
      std::cerr << "PMEM_END_ADDR not set" << std::endl;
      exit(1);
    }
    
    start_addr = (void*)std::stoull(start_addr_str, 0, 16);
    end_addr = (void*)std::stoull(end_addr_str, 0, 16);
    
    assert(start_addr != nullptr);
    assert(end_addr != nullptr);
    
    assert(start_addr < end_addr);

    printf("Values = %s, %s\n", start_addr_str.c_str(), end_addr_str.c_str());
  }

  __attribute__((__constructor__))
  void libstoreinst_ctor() {
    init_dlsyms();
    init_counters();
    init_addrs();

    cxlModeEnabled = get_env_val("CXL_MODE_ENABLED");

    printf("Address range = [%p:%p]\n", start_addr, end_addr);

    const std::string buf_f = "/mnt/pmem0/buf";
    const int fd = open(buf_f.c_str(), O_RDWR);

    if (fd == -1) {
      perror("open failed");
      exit(1);
    }
  
    log_area = (char*)mmap((char*)end_addr-(BUF_SIZE*2), BUF_SIZE,
                           PROT_READ | PROT_WRITE,
                           MAP_SYNC | MAP_SHARED_VALIDATE, fd, 0);

    if (log_area == (char*)-1) {
      perror("mmap failed");
      exit(1);
    }

    const int pm_backing_fd = open("/mnt/pmem0/pm-backing", O_RDWR);
    if (pm_backing_fd == -1) {
      perror("open(\"/mnt/pmem0/pm-backing\") failed");
      exit(1);
    }

    pm_back = (char*)mmap(NULL, 1024*1024*1024, PROT_READ | PROT_WRITE,
                          MAP_SYNC | MAP_SHARED_VALIDATE, pm_backing_fd, 0);

    if (pm_back == (char*)-1) {
      perror("mmap(pm_backing_fd) failed");
      exit(1);
    }
  
    printf("Mounted log area at %p\n", (void*)log_area);
  }

  void log_range(void *start, size_t bytes) {
    auto &log_entry = *(log_t*)((char*)log_area + current_log_off);
    
    if (log_area != nullptr and cxlModeEnabled) {
      storeInstEnabled = true;
      
      // printf("checking mem %p\n", ptr);
      log_entry.addr = (uint64_t)start;
      log_entry.bytes = bytes;

      real_memcpy(&log_entry.val, start, bytes);

      const auto cl_count = (sizeof(log_t) + bytes+63)/64;
      size_t cur_cl = 0;
      while (cur_cl < cl_count) {
        _mm_clwb((char*)&log_entry + cur_cl*64);
        cur_cl++;
      }
      _mm_sfence();
      ++*logged_check_count;
      current_log_off += sizeof(log_t) + bytes;

      assert(current_log_off < BUF_SIZE);

    }
  }
  
  __attribute__((unused))
  void checkMemory(void* ptr) {
    if (startTracking) {
      if (start_addr < ptr and ptr < end_addr) {
        log_range(ptr, 8);
      } else {
        ++*skip_check_count;
      }
      }
  }

  __attribute__((destructor))
  void libstoreinst_dtor() {
    std::cerr << "Summary:\n";
    std::cerr << skip_check_count->str() << "\n";
    std::cerr << logged_check_count->str() << "\n";
    std::cerr << tx_log_count_dist->str() << "\n";
  }

  __attribute__((unused))
  int snapshot(void *addr, size_t bytes, int flags) {
    if (storeInstEnabled) {
      auto log = (log_t*)log_area;
      size_t total_proc = 0;
      for (size_t i = 0; i < current_log_off;) {
        auto &log_entry = *(log_t*)((char*)log + i);
        
        if (log_entry.addr != 0 and log_entry.addr < (uint64_t)addr + bytes
            and log_entry.addr >= (uint64_t)addr) {
          const size_t offset = log_entry.addr - (uint64_t)start_addr;
          const size_t dst_addr = (size_t)(pm_back + offset);

          *(uint64_t*)dst_addr = *(uint64_t*)log_entry.addr;

          log_entry.addr = 0;
          
          _mm_clwb((void*)dst_addr);
        } else if (log_entry.addr == 0) {
          break;
        }

        total_proc++;
        i += sizeof(log_t) + log_entry.bytes;
      }
      tx_log_count_dist->add(total_proc);
      _mm_sfence();
        
    } else {
      void *pg_aligned = (void*)(((size_t)addr>>12)<<12);

      int mret = msync(pg_aligned, bytes, flags);

      if (-1 == mret) {
        perror("msync for snapshot failed");
        exit(1);
      }
    }
    current_log_off = 0;
    return 0;
  }

  void *memcpy(void *__restrict dst, const void *__restrict src, size_t n) {
    if (startTracking and start_addr != nullptr and start_addr <= dst and dst < end_addr) {
      log_range(dst, n);
    }
    
    return real_memcpy(dst, src, n);
  }

  void *memmove(void *__restrict dst, const void *__restrict src, size_t n) {
    if (startTracking and start_addr != nullptr and start_addr <= dst and dst < end_addr) {
      log_range(dst, n);
    }

    return real_memmove(dst, src, n);
  }
}
