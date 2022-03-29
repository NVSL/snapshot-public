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
#include "nvsl/pmemops.hh"
#include "nvsl/stats.hh"
#include "nvsl/trace.hh"

NVSL_DECL_ENV(PMEM_START_ADDR);
NVSL_DECL_ENV(PMEM_END_ADDR);
NVSL_DECL_ENV(ENABLE_CHECK_MEMORY_TRACING);

extern "C" {
  void *start_addr, *end_addr = nullptr;
  char *log_area;
  size_t current_log_off = 0;
  bool startTracking = false;
  bool storeInstEnabled = false;
  bool cxlModeEnabled = false;
  nvsl::PMemOps *pmemops;
  std::ofstream *traceStream;
  
  void init_counters() {
    skip_check_count = new nvsl::Counter();
    logged_check_count = new nvsl::Counter();
    tx_log_count_dist = new nvsl::StatsFreq<>();

    skip_check_count->init("skip_check_count", "Skipped memory checks");
    logged_check_count->init("logged_check_count", "Logged memory checks");
    tx_log_count_dist->init("tx_log_count_dist", 
                            "Distribution of number of logs in a transaction",
                            5, 0, 30);
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

  void init_pmemops() {
    pmemops = new nvsl::PMemOpsClwb();
  }

  __attribute__((__constructor__))
  void libstoreinst_ctor() {
    init_dlsyms();
    init_counters();
    init_addrs();
    init_pmemops();

    traceStream = new std::ofstream("/tmp/stacktrace");

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
  
    printf("Mounted log area at %p\n", (void*)log_area);
  }
  
  __attribute__((unused))
  void checkMemory(void* ptr) {
    if (startTracking) {
      if (start_addr < ptr and ptr < end_addr) {
        log_range(ptr, 8);
#ifndef RELEASE
        if (get_env_val(ENABLE_CHECK_MEMORY_TRACING_ENV)) {
          *traceStream << "-----\n" << nvsl::get_stack_trace() << "\n\n";
        }
#endif
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
}
