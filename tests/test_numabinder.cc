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

#include "libcxlfs/numabinder.hh"
#include "nvsl/common.hh"
#include "nvsl/utils.hh"

using nvsl::RCast;

NumaBinder *nb;

TEST(init, numabinder) {
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
