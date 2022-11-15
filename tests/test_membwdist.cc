// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   test_membwdist.cc
 * @date   novembre  9, 2022
 * @brief  Brief description here
 */

#include "gtest/gtest.h"
#include <cstdlib>
#include <iostream>
#include <pthread.h>
#include <sys/mman.h>

#include "libcxlfs/membwdist.hh"
#include "nvsl/common.hh"
#include "nvsl/utils.hh"

MemBWDist *mbd;

TEST(init, membwdist) {
  mbd = new MemBWDist();
  mbd->start_sampling(2);
}
