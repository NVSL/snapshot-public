// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   clwbflushdist.cc
 * @date   avril 10, 2022
 * @brief  Brief description here
 */

#include "nvsl/clock.hh"
#include "nvsl/pmemops.hh"
#include "run.hh"

#include <map>
#include <sys/mman.h>

using namespace nvsl;

void mb_clwbsfencedist() {
  PMemOpsClwb pmemops;

  int fd = open("/mnt/pmem0/microbench", O_CREAT | O_RDWR, 0666);
  if (fd == -1) {
    DBGE << "Unable to open the microbenchmark file" << std::endl;
    DBGE << PSTR();
    exit(1);
  }

  constexpr size_t MMAP_SIZE = 100 * 1024 * 4096;
  constexpr size_t AVERAGE_CNT = 16 * 1024 * 1024;

  lseek(fd, MMAP_SIZE + 1, SEEK_SET);
  write(fd, 0, 1);
  lseek(fd, 0, SEEK_SET);

  void *pm = mmap(nullptr, MMAP_SIZE, PROT_READ | PROT_WRITE,
                  MAP_SHARED_VALIDATE | MAP_SYNC, fd, 0);
  memset(pm, 0, MMAP_SIZE);

  auto arr = (uint64_t *)pm;

  constexpr size_t MAX_UPDATES = 600;

  std::map<size_t, std::array<double, 3>> results;

  for (size_t i = 0; i < 400; i += 10) {
    uint64_t val[8];
    memset(val, rand(), sizeof(val));

    Clock clk;
    clk.tick();
    for (size_t avg_i = 0; avg_i < AVERAGE_CNT; avg_i++) {
      size_t j = 0;

      std::memcpy(&arr[0], val, sizeof(val));

      for (; j < i; j++) {
        arr[4096 + j * 64] = j;
      }
      pmemops.flush(&arr[0], sizeof(val));
      pmemops.drain();

      for (; j < MAX_UPDATES; j++) {
        arr[4096 + j * 64] = j;
      }
    }
    clk.tock();

    results[i][0] = clk.ns() / (double)AVERAGE_CNT;
    std::cerr << results[i][0] << std::endl;
  }

  std::cerr << "=======" << std::endl;

  for (size_t i = 0; i < 400; i += 10) {
    uint64_t val[8];
    memset(val, rand(), sizeof(val));

    Clock clk;
    clk.tick();
    for (size_t avg_i = 0; avg_i < AVERAGE_CNT; avg_i++) {
      size_t j = 0;

      std::memcpy(&arr[0], val, sizeof(val));
      pmemops.flush(&arr[0], sizeof(val));

      for (; j < i; j++) {
        arr[4096 + j * 64] = j;
      }
      pmemops.drain();

      for (; j < MAX_UPDATES; j++) {
        arr[4096 + j * 64] = j;
      }
    }
    clk.tock();

    results[i][1] = clk.ns() / (double)AVERAGE_CNT;
    std::cerr << results[i][1] << std::endl;
  }

  std::cerr << "=======" << std::endl;

  for (size_t i = 0; i < 400; i += 10) {
    uint64_t val[8];
    memset(val, rand(), sizeof(val));

    Clock clk;
    clk.tick();
    for (size_t avg_i = 0; avg_i < AVERAGE_CNT; avg_i++) {
      size_t j = 0;

      pmemops.streaming_wr(&arr[0], val, sizeof(val));

      for (; j < i * 1; j++) {
        arr[4096 + j * 64] = j;
      }
      pmemops.drain();

      for (; j < MAX_UPDATES; j++) {
        arr[4096 + j * 64] = j;
      }
    }
    clk.tock();

    results[i][2] = clk.ns() / (double)AVERAGE_CNT;
    std::cerr << results[i][2] << std::endl;
  }

  std::cerr << "=======" << std::endl;

  for (const auto &[k, v] : results) {
    std::cout << k << ", " << v[0] << ", " << v[1] << ", " << v[2] << std::endl;
  }
}
