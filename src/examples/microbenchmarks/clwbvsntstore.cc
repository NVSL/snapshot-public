// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   clwbvsntstore.cc
 * @date   Mai 4, 2022
 * @brief  Brief description here
 */

#include "nvsl/clock.hh"
#include "nvsl/pmemops.hh"
#include "run.hh"
#include <sys/mman.h>

using namespace nvsl;

void mb_clwbvsntstore() {
  PMemOpsClwb pmemops;

  int fd = open("/mnt/pmem0/microbench", O_CREAT | O_RDWR, 0666);
  if (fd == -1) {
    DBGE << "Unable to open the microbenchmark file" << std::endl;
    DBGE << PSTR();
    exit(1);
  }

  constexpr size_t MMAP_SIZE = 100 * 1024 * 4096;

  lseek(fd, MMAP_SIZE + 1, SEEK_SET);
  write(fd, 0, 1);
  lseek(fd, 0, SEEK_SET);

  void *pm = mmap(nullptr, MMAP_SIZE, PROT_READ | PROT_WRITE,
                  MAP_SHARED_VALIDATE | MAP_SYNC, fd, 0);
  memset(pm, 0, MMAP_SIZE);

  auto arr = (uint64_t *)pm;

  constexpr size_t MAX_UPDATES = 6000;

  uint64_t cacheline[8];
  memset(&cacheline[0], rand(), 8);

  Clock clk;
  clk.tick();
  for (size_t i = 0; i < MAX_UPDATES; i += 1) {
    int idx = rand() % (MMAP_SIZE / sizeof(uint64_t));
    idx = idx / 8;
    idx = idx * 8;
    arr[idx] = cacheline[0];
    memcpy(&arr[idx], &cacheline[0], sizeof(cacheline));
    pmemops.flush(&arr[0], 8 * sizeof(uint64_t));
    pmemops.drain();
  }
  clk.tock();

  std::cout << clk.ns() / (double)MAX_UPDATES << std::endl;

  memset(&cacheline[0], rand(), 8);

  clk.reset();
  clk.tick();

  for (size_t i = 0; i < MAX_UPDATES; i += 1) {
    int idx = rand() % (MMAP_SIZE / sizeof(uint64_t));
    idx = idx / 8;
    idx = idx * 8;

    arr[idx] = idx;
    pmemops.streaming_wr(&arr[0], cacheline, sizeof(cacheline));
    pmemops.drain();
  }
  clk.tock();

  std::cout << clk.ns() / (double)MAX_UPDATES << std::endl;
}
