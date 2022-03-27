#include <cassert>
#include <fcntl.h>
#include <immintrin.h>
#include <iostream>
#include <numeric>
#include <sys/mman.h>

#include "common.hh"
#include "libc_wrappers.hh"
#include "libstoreinst.hh"
#include "log.hh"
#include "nvsl/common.hh"
#include "nvsl/envvars.hh"
#include "nvsl/stats.hh"

NVSL_DECL_ENV(PMEM_START_ADDR);
NVSL_DECL_ENV(PMEM_END_ADDR);

extern "C" {
  void *start_addr, *end_addr = nullptr;
  char *log_area, *pm_back;
  size_t current_log_off = 0;
  bool startTracking = false;
  bool storeInstEnabled = false;
  bool cxlModeEnabled = false;

  
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
  
    log_area = (char*)real_mmap((char*)end_addr-(BUF_SIZE*2), BUF_SIZE,
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

    pm_back = (char*)real_mmap(NULL, 1024*1024*1024, PROT_READ | PROT_WRITE,
                          MAP_SYNC | MAP_SHARED_VALIDATE, pm_backing_fd, 0);

    if (pm_back == (char*)-1) {
      perror("mmap(pm_backing_fd) failed");
      exit(1);
    }
  
    printf("Mounted log area at %p\n", (void*)log_area);
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
      // std::cout << "total_proc = " << total_proc << "\n";
      
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
}
