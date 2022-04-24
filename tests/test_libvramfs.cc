// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   test_libvram.cc
 * @date   Avril 24, 2022
 * @brief  Test libvramfs implementation
 */

#include "gtest/gtest.h"
#include <cstdlib>
#include <iostream>

#include "libvramfs/libvramfs.hh"
#include "nvsl/utils.hh"

TEST(memcheck, libvramfs) {
  int fd = nvsl::libvramfs::open("/mnt/pmem0/", O_CREAT | O_RDWR);
  ASSERT_TRUE(fd != -1);

  off_t seek = nvsl::libvramfs::lseek(fd, 4096, SEEK_SET);
  ASSERT_TRUE(seek != -1);

  void *addr = nvsl::libvramfs::mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
                                     MAP_SHARED, fd, 0);
  ASSERT_TRUE(addr != MAP_FAILED);
  memset(addr, 0, 4096);
}
