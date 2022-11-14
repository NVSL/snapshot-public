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
#include "nvsl/common.hh"
#include "nvsl/utils.hh"

UserBlkDev *ubd;

TEST(init, blkdev) {
  ubd = new UserBlkDev();
  ASSERT_TRUE(ubd->init() == 0);
}

TEST(read_0, blkdev) {
  char *buf = (char *)malloc(0x1000);
  int rc = ubd->read_blocking(buf, 0, 1);

  ASSERT_NE(rc, -1);
}

TEST(write_and_read_0, blkdev) {
  char *ref_buf, *buf;
  int rc = 0;

  ref_buf = (char *)aligned_alloc(0x1000, 0x1000);
  memset(ref_buf, 0xff, 0x1000);
  rc = mprotect(ref_buf, 0x1000, PROT_READ);
  if (rc == -1) {
    perror("mprotect");
  }

  ASSERT_NE(rc, -1);

  buf = (char *)malloc(0x1000);
  memset(buf, 0x0, 0x1000);

  rc = ubd->write_blocking(ref_buf, 0, 8);
  ASSERT_NE(rc, -1);

  rc = ubd->read_blocking(buf, 0, 8);
  ASSERT_NE(rc, -1);

  ASSERT_EQ(memcmp(buf, ref_buf, 0x1000), 0);
}
