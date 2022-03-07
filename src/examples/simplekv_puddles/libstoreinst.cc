#include <fcntl.h>
#include <iostream>
#include "common.hh"
#include <cassert>
#include <sys/mman.h>
#include <immintrin.h>

#define PMEM_START_ADDR_ENV "PMEM_START_ADDR"
#define PMEM_END_ADDR_ENV "PMEM_END_ADDR"
#define BUF_SIZE (1000*4096)

static void *start_addr, *end_addr = nullptr;
char *log_area, *pm_back;
size_t current_log_off = 0;
bool startTracking = false;
bool storeInstEnabled = false;

struct log_t {
  uint64_t addr;
  uint64_t val;
};

__attribute__((constructor)) void libstoreinst_ctor() {
  const auto start_addr_str = get_env_str(PMEM_START_ADDR_ENV);
  const auto end_addr_str = get_env_str(PMEM_END_ADDR_ENV);

  if (start_addr_str == "") {
    fprintf(stderr, "%s not set\n", PMEM_START_ADDR_ENV);
    exit(1);
  }

  if (end_addr_str == "") {
    fprintf(stderr, "%s not set\n", PMEM_END_ADDR_ENV);
    exit(1);
  }

  printf("Values = %s, %s\n", start_addr_str.c_str(), end_addr_str.c_str());

  start_addr = (void*)std::stoull(start_addr_str, 0, 16);
  end_addr = (void*)std::stoull(end_addr_str, 0, 16);

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

extern "C" {
  void checkMemory(void* ptr) {
    storeInstEnabled = true;
    auto log = (log_t*)log_area;

    if (start_addr < ptr and ptr < end_addr and startTracking) {
      log[current_log_off].addr = (uint64_t)ptr;
      log[current_log_off].val = *(uint64_t*)ptr;

      current_log_off++;

      assert(current_log_off < BUF_SIZE/sizeof(log_t));
    }
  }

  int snapshot(void *addr, size_t bytes, int flags) {
    if (storeInstEnabled) {
      auto log = (log_t*)log_area;
      for (size_t i = 0; i < current_log_off; i++) {
        if (log[i].addr != 0) {
          const size_t offset = log[i].addr - (uint64_t)start_addr;
          const size_t dst_addr = (size_t)(pm_back + offset);

          *(uint64_t*)dst_addr = *(uint64_t*)log[i].addr;
          _mm_clwb((void*)dst_addr);
        }
      }
      _mm_sfence();
        
    } else {
      // printf("Calling actual msync\n");
      msync(addr, bytes, flags);
    }
    current_log_off = 0;
    return 0;
  }
}
