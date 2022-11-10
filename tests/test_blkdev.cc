// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   test_blkdev.cc
 * @date   novembre  9, 2022
 * @brief  Brief description here
 */

#include "gtest/gtest.h"
#include <cstdlib>
#include <iostream>

#include "libcxlfs/blkdev.hh"
#include "nvsl/utils.hh"

UserBlkDev *ubd;

TEST(init, blkdev) {
  ubd = new UserBlkDev();
  ASSERT_TRUE(ubd->init() == 0);
}
