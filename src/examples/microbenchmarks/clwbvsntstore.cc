// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   clwbvsntstore.cc
 * @date   Mai 4, 2022
 * @brief  Brief description here
 */

#include "nvsl/clock.hh"
#include "nvsl/constants.hh"
#include "nvsl/pmemops.hh"
#include "nvsl/utils.hh"
#include "run.hh"

#include <map>
#include <set>
#include <sys/mman.h>

using namespace nvsl;

constexpr size_t MAX_UPDATES = 6000;
constexpr size_t FLUSH_SZ_START = 1;  // Cacheliness
constexpr size_t FLUSH_SZ_END = 128;  // 1024; // Cacheliness
constexpr size_t DRAIN_DST_START = 1; // Writes
constexpr size_t DRAIN_DST_END = 256; // Writes
constexpr size_t DRAIN_DST_INCR = 2;  // Writes

void mb_clwbvsntstore() {
  PMemOpsClwb pmemops;
  PMemOpsClflushOpt pmemopsClflush;

  int fd = open("/mnt/pmem0/microbench", O_CREAT | O_RDWR, 0666);
  if (fd == -1) {
    DBGE << "Unable to open the microbenchmark file" << std::endl;
    DBGE << PSTR();
    exit(1);
  }

  constexpr size_t MMAP_SIZE = 1024UL * 1024 * 4096;

  lseek(fd, MMAP_SIZE + 1, SEEK_SET);
  write(fd, 0, 1);
  lseek(fd, 0, SEEK_SET);

  void *pm = mmap(nullptr, MMAP_SIZE, PROT_READ | PROT_WRITE,
                  MAP_SHARED_VALIDATE | MAP_SYNC, fd, 0);
  memset(pm, 1, MMAP_SIZE);
  auto arr = (uint8_t *)pm;

  char cacheline[FLUSH_SZ_END * 64];
  memset(cacheline, rand(), sizeof(cacheline));

  std::map<size_t, std::array<std::vector<double>, 2>> results;

  std::cerr << "Setup complete\n";

  Clock clk;
  for (size_t flush_sz = FLUSH_SZ_START; flush_sz < FLUSH_SZ_END;
       flush_sz *= 2) {
    const size_t flush_sz_bytes = nvsl::CL_SIZE * flush_sz;

    for (size_t drain_dst = DRAIN_DST_START; drain_dst < DRAIN_DST_END;
         drain_dst *= DRAIN_DST_INCR) {
      clk.reset();
      clk.tick();
      for (size_t i = 0; i < MAX_UPDATES; i += 1) {
        const size_t idx = nvsl::round_up(rand() % (MMAP_SIZE - flush_sz_bytes),
                                          flush_sz_bytes);

        memcpy(&arr[idx], cacheline, flush_sz_bytes);
        pmemops.flush(&arr[idx], flush_sz_bytes);

        if (i % drain_dst == 0) pmemops.drain();
      }
      clk.tock();

      pmemops.flush(pm, MMAP_SIZE);
      pmemops.drain();

      results[flush_sz_bytes][0].push_back(clk.ns() / (double)MAX_UPDATES);
    }
  }

  std::cerr << "Completed first loop" << std::endl;

  pmemopsClflush.flush(pm, MMAP_SIZE);
  pmemops.drain();

  for (size_t flush_sz = FLUSH_SZ_START; flush_sz < FLUSH_SZ_END;
       flush_sz *= 2) {
    const size_t flush_sz_bytes = nvsl::CL_SIZE * flush_sz;

    for (size_t drain_dst = DRAIN_DST_START; drain_dst < DRAIN_DST_END;
         drain_dst *= DRAIN_DST_INCR) {
      clk.reset();
      clk.tick();
      for (size_t i = 0; i < MAX_UPDATES; i += 1) {
        const size_t idx = nvsl::round_up(rand() % (MMAP_SIZE - flush_sz_bytes),
                                          flush_sz_bytes);

        pmemops.streaming_wr(&arr[idx], cacheline, flush_sz_bytes);
        if (i % drain_dst == 0) pmemops.drain();
      }
      clk.tock();
      results[flush_sz_bytes][1].push_back(clk.ns() / (double)MAX_UPDATES);
    }
  }

  /* Generate results */
  for (const auto &[k, vs] : results) {
    NVSL_ASSERT(vs[0].size() == vs[1].size(),
                "Both vectors should be same sized");

    size_t drain_dist = DRAIN_DST_START;
    for (size_t elem_i = 0; elem_i < vs[0].size(); elem_i++) {
      std::cout << k << ", " << drain_dist << ", " << vs[0][elem_i] << ", "
                << vs[1][elem_i] << std::endl;
      drain_dist *= DRAIN_DST_INCR;
    }
  }
}
