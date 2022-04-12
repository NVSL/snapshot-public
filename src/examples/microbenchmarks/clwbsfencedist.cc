// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   clwbflushdist.cc
 * @date   avril 10, 2022
 * @brief  Brief description here
 */

#include "run.hh"
#include "nvsl/pmemops.hh"
#include "nvsl/clock.hh"
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

  constexpr size_t MMAP_SIZE = 100*1024*4096;
  constexpr size_t AVERAGE_CNT = 10*1024*1024;
  
  lseek(fd, MMAP_SIZE+1, SEEK_SET);
  write(fd, 0, 1);
  lseek(fd, 0, SEEK_SET);


  void *pm = mmap(nullptr, MMAP_SIZE, PROT_READ | PROT_WRITE, 
                  MAP_SHARED_VALIDATE | MAP_SYNC, fd, 0);
  memset(pm, 0, MMAP_SIZE);
  
  auto arr = (uint64_t *)pm;

  constexpr size_t MAX_UPDATES = 600;
  
  for (size_t i = 0; i < 600; i+=10) {
    int val = rand();
    Clock clk;
    clk.tick();
    for (size_t avg_i = 0; avg_i < AVERAGE_CNT; avg_i++) {
      size_t j = 0;

      for (size_t u_i = 0; u_i < 64; u_i++) {
        arr[u_i * 64] = val;
      }

      pmemops.flush(&arr[0], 64 * sizeof(uint64_t));

      for (; j < i * 1; j++) {
        arr[4096 + j * 64] = j;
      }
      pmemops.drain();

      for (; j < MAX_UPDATES; j++) {
        arr[4096 + j * 64] = j;
      }
    
    }
    clk.tock();

    std::cout << i << "," << clk.ns()/(double)AVERAGE_CNT << std::endl;
  }
}

