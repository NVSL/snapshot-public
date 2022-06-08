// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   msyncscaling.cc
 * @date   Mai 19, 2022
 * @brief  Understand the scaling behavior of msync()
 */

#include "nvsl/clock.hh"
#include "nvsl/constants.hh"
#include "nvsl/envvars.hh"
#include "nvsl/pmemops.hh"
#include "nvsl/utils.hh"
#include "run.hh"

#include <fcntl.h>
#include <map>
#include <pthread.h>
#include <set>
#include <sys/mman.h>

using namespace nvsl;

constexpr size_t MAX_LOOPS = 500000;
constexpr size_t MAX_UPDATES = 3;
constexpr size_t MAX_THREADS = 20;
constexpr size_t MMAP_SIZE = 1024UL * 1024 * 1024;

PMemOpsClwb pmemops;

struct thread_arg_t {
  size_t tid;
  void *mem_region;
  size_t total_threads;
};

void *msync_thread(void *vargp) {
  auto *ta = RCast<thread_arg_t *>(vargp);

  char *arr = (char *)ta->mem_region;

  volatile size_t malloc_sz = 64 * 8;
  char *cacheline = (char *)malloc(malloc_sz);
  memset(cacheline, rand(), malloc_sz);

  for (size_t loop = 0; loop < MAX_LOOPS / ta->total_threads; loop++) {
    for (size_t i = 0; i < MAX_UPDATES; i++) {
      auto dst = &arr[((rand() % MMAP_SIZE) >> 9) << 9];
      auto src = cacheline;
      memcpy(dst, src, malloc_sz);
    }

    if (-1 == msync(ta->mem_region, MMAP_SIZE, MS_SYNC)) {
      DBGE << "snapshot call failed" << std::endl;
      exit(1);
    }
    pmemops.drain();
  }

  return nullptr;
}

std::vector<void *> allocate_mem_regions() {
  std::vector<void *> results;

  for (size_t tid = 0; tid < MAX_THREADS; tid++) {
    std::string fname = get_env_str("BENCHMARK_FILE");
    if (fname.empty()) {
      DBGE << "Use env val BENCHMARK_FILE to specify the file to use for the "
              "benchmark\n";
      exit(1);
    }
    fname += S(tid);
    int fd = open(fname.c_str(), O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
      DBGE << "Unable to open the microbenchmark file" << std::endl;
      DBGE << PSTR();
      exit(1);
    }
    lseek(fd, MMAP_SIZE + 1, SEEK_SET);
    if (1 != write(fd, "\0", 1)) {
      DBGE << "write failed to file " << fname << "\n";
      DBGE << PSTR() << "\n";
      exit(1);
    }
    lseek(fd, 0, SEEK_SET);

    fsync(fd);

    void *pm =
        mmap(nullptr, MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (pm == MAP_FAILED) {
      DBGE << "mmap failed: \n";
      DBGE << PSTR();
      exit(1);
    }

    int madv_flag = 0;
    if (get_env_val("CXLBUF_USE_HUGEPAGE")) {
      madv_flag = MADV_HUGEPAGE;
    } else {
      madv_flag = MADV_NOHUGEPAGE;
    }

    if (-1 == madvise(pm, MMAP_SIZE, madv_flag)) {
      DBGE << "madvise() failed\n";
      DBGE << PSTR() << "\n";
      exit(1);
    }

    memset(pm, 1, MMAP_SIZE);
    results.push_back(pm);
  }

  return results;
}

void mb_msyncscaling() {
  // std::map<size_t, std::array<std::vector<double>, 2>> results;

  auto mem_regions = allocate_mem_regions();

  std::cerr << "Setup complete\n";

  const thread_arg_t *ta = new thread_arg_t(
      {.tid = 0, .mem_region = mem_regions[0], .total_threads = 1});

  msync_thread((void *)ta);

  Clock clk;
  for (size_t tcount = 1; tcount < MAX_THREADS; tcount++) {
    clk.reset();
    clk.tick();
    std::vector<pthread_t> tids;
    for (size_t tid = 0; tid < tcount; tid++) {
      tids.push_back(0);

      const thread_arg_t *ta = new thread_arg_t({.tid = tid,
                                                 .mem_region = mem_regions[tid],
                                                 .total_threads = tcount});

      pthread_create(&tids[tid], NULL, msync_thread, (void *)ta);
    }

    for (size_t tid = 0; tid < tcount; tid++) {
      pthread_join(tids[tid], NULL);
    }
    clk.tock();
    std::cout << tcount << ", " << clk.ns() << "\n";

    sync();
    int fd = open("/proc/sys/vm/drop_caches", O_WRONLY);
    write(fd, "1", 1);
    close(fd);
  }

  // /* Generate results */
  // for (const auto &[k, vs] : results) {
  //   NVSL_ASSERT(vs[0].size() == vs[1].size(),
  //               "Both vectors should be same sized");

  //   size_t drain_dist = DRAIN_DST_START;
  //   for (size_t elem_i = 0; elem_i < vs[0].size(); elem_i++) {
  //     std::cout << k << ", " << drain_dist << ", " << vs[0][elem_i] << ", "
  //               << vs[1][elem_i] << std::endl;
  //     drain_dist *= DRAIN_DST_INCR;
  //   }
  // }
}
