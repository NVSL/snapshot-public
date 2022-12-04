// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   test_membwdist.cc
 * @date   novembre  9, 2022
 * @brief  Brief description here
 */

#include "gtest/gtest.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <pthread.h>
#include <sys/mman.h>
#include <tuple>
#include <utility>

#include "libcxlfs/membwdist.hh"
#include "nvsl/common.hh"
#include "nvsl/utils.hh"

using nvsl::RCast;

MemBWDist *mbd;

std::pair<uint64_t, uint64_t> access_mem() {
  constexpr size_t alloc_sz = 1UL << 32;
  char *large_arr =
      (char *)mmap(nullptr, alloc_sz, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
  uint64_t large_arr_end = (uint64_t)large_arr + alloc_sz;

  volatile size_t val = 0;
  for (uint64_t i = 0; i < (0x1UL << 28); i++) {
    const auto scale = (size_t)pow(2, (rand() % 30));
    auto idx = scale + (rand() % (64 * 0x100));
    idx = idx % alloc_sz;
    val = val + large_arr[idx] + 10;
  }
  (void)val;

  munmap(large_arr, alloc_sz);

  return std::make_pair(RCast<uint64_t>(large_arr), large_arr_end);
}

TEST(membwdist, init) {
  int rc;

  mbd = new MemBWDist();
  rc = mbd->start_sampling(1);

  ASSERT_EQ(rc, 0);
}

TEST(membwdist, access_and_sample) {

  const auto [start, end] = access_mem();
  DBGH(0) << "pages: " << ((end - start) >> 12) << "\n";
  const auto dist = mbd->get_dist(start, end);
  ASSERT_GT(dist.size(), 1024);
}
