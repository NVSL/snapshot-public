// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   msyncscaling.cc
 * @date   Mai 19, 2022
 * @brief  Understand the scaling behavior of msync()
 */

#include "libstoreinst.hh"
#include "nvsl/clock.hh"
#include "nvsl/constants.hh"
#include "nvsl/pmemops.hh"
#include "nvsl/utils.hh"
#include "run.hh"

#include "famus.h"

#include <map>
#include <pthread.h>
#include <set>
#include <sys/mman.h>

using namespace nvsl;

constexpr size_t MAX_LOOPS = 100;
constexpr size_t MAX_UPDATES = 2;
constexpr size_t MAX_THREADS = 1;
constexpr size_t MMAP_SIZE = 1024UL * 1024 * 256;

struct thread_arg_t {
  size_t tid;
  int fd;
  void *mem_region;
  bool use_real_msync;
};

void /***/ msync_thread(void *vargp) {
  auto *ta = RCast<thread_arg_t *>(vargp);

  char *arr = (char *)ta->mem_region;

  volatile size_t malloc_sz = 64 * 8;
  char *cacheline = (char *)malloc(malloc_sz);
  memset(cacheline, rand(), malloc_sz);

  for (size_t loop = 0; loop < MAX_LOOPS; loop++) {
    for (size_t i = 0; i < MAX_UPDATES; i++) {
      auto dst = &arr[((rand() % MMAP_SIZE) >> 9) << 9];
      auto src = cacheline;
      memcpy(dst, src, malloc_sz);
    }

    if (ta->use_real_msync) {
      msync(ta->mem_region, MMAP_SIZE, MS_SYNC);
    } else if (-1 == famus_snap_sync(ta->fd)) {
      DBGE << "snapshot call failed" << std::endl;
      exit(1);
    }
  }
}

std::vector<std::pair<void *, int>> allocate_mem_regions() {
  std::vector<std::pair<void *, int>> results;

  for (size_t tid = 0; tid < MAX_THREADS; tid++) {
    std::string fname = "/mnt/pmem0p4/microbench." + S(tid);
    int fd = open(fname.c_str(), O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
      DBGE << "Unable to open the microbenchmark file" << std::endl;
      DBGE << PSTR();
      exit(1);
    }
    if (const auto ret = lseek(fd, MMAP_SIZE + 1, SEEK_SET);
        ret != MMAP_SIZE + 1) {
      DBGE << "lseek failed, got " << ret << ", expected " << MMAP_SIZE + 1
           << std::endl;
      DBGE << PSTR();
      exit(1);
    }

    if (1 != write(fd, "0", 1)) {
      DBGE << "write failed\n";
      DBGE << PSTR();
      exit(1);
    }

    lseek(fd, 0, SEEK_SET);

    if (-1 == fsync(fd)) {
      DBGE << "fsync failed\n";
      DBGE << PSTR() << "\n";
      exit(1);
    }

    void *pm =
        mmap(nullptr, MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (pm == MAP_FAILED) {
      DBGE << "mmap failed"
           << "\n";
      DBGE << PSTR() << "\n";
      exit(1);
    }
    memset(pm, 1, MMAP_SIZE);
    results.push_back(std::make_pair(pm, fd));

    int madv_flag = MADV_NOHUGEPAGE;
    if (-1 == madvise(pm, MMAP_SIZE, madv_flag)) {
      DBGE << "madvise() failed\n";
      DBGE << PSTR() << "\n";
      exit(1);
    }
  }

  msync((void *)results[0].first, MMAP_SIZE, MS_SYNC);

  return results;
}

void mb_msyncscaling(bool use_real_msync) {
  auto mem_regions = allocate_mem_regions();

  if (use_real_msync == true) {
    DBGE << "Real msync disabled\n";
    exit(1);
  }

  const thread_arg_t *ta = new thread_arg_t({.tid = 0,
                                             .fd = mem_regions[0].second,
                                             .mem_region = mem_regions[0].first,
                                             .use_real_msync = use_real_msync});

  Clock clk;
  clk.tick();
  msync_thread((void *)ta);
  clk.tock();

  //  double elapsed_us = clk.ns() / 1000.0;

  // std::cout << 0 << ", " << elapsed_us / (double)(MAX_LOOPS) << "\n";
}
