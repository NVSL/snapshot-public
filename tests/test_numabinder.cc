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
#include <numa.h>
#include <pthread.h>
#include <sys/mman.h>
#include <tuple>
#include <utility>

#include "libcxlfs/numabinder.hh"
#include "nvsl/common.hh"
#include "nvsl/utils.hh"

using nvsl::RCast;

NumaBinder *nb;

TEST(numabinder, init) {
  int rc = -1;

  nb = new NumaBinder;

  nb->bind_to_node(0);
  rc = nb->bind_to_free_cpu();

  ASSERT_NE(rc, -1);

  ASSERT_EQ(sched_getcpu(), rc);

  rc = nb->bind_to_free_cpu(2);

  ASSERT_NE(rc, -1);

  ASSERT_EQ(sched_getcpu(), 2);

  rc = nb->bind_to_free_cpu(4);

  ASSERT_NE(rc, -1);

  ASSERT_EQ(sched_getcpu(), 4);
}

TEST(numabinder, numa_node) {

  nb->bind_to_node(0);

  if (numa_available() != -1) {
    const auto tgt_node = numa_max_node();
    std::cerr << "tgt_node = " << tgt_node << std::endl;

    if (tgt_node > 0) {
      /* We have at least two NUMA nodes */

      ASSERT_NE(NumaBinder::get_cur_numa_node(), tgt_node);

      nb->bind_to_node(tgt_node);

      ASSERT_EQ(NumaBinder::get_cur_numa_node(), tgt_node);
    } else {
      ASSERT_EQ(NumaBinder::get_cur_numa_node(), 0);
      std::cerr << "Skipping NUMA tests, only one node found" << std::endl;
    }
  }
}
