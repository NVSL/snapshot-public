// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   test_libvram.cc
 * @date   Avril 23, 2022
 * @brief  Test libvram implementation
 */

#include "gtest/gtest.h"
#include <cstdlib>
#include <iostream>

#include "libvram/libvram.hh"
#include "nvsl/utils.hh"

TEST(memcheck, libvram) {
  constexpr size_t BUF_SZ = 1UL * 1024 * 1024 * 1024;
  char *buf_bptr = (char *)nvsl::libvram::vram_malloc(BUF_SZ);

  ASSERT_TRUE(nvsl::memcheck(buf_bptr, BUF_SZ) == 0);
}
