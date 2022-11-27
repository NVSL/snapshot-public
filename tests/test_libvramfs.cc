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

constexpr size_t MIN_ALLOC_SZ = 2UL * 1024 * 1024;

TEST(libvramfs, memcheck) {
  int fd = nvsl::libvramfs::open("/mnt/pmem0/a", O_CREAT | O_RDWR);
  ASSERT_TRUE(fd != -1);

  off_t seek = nvsl::libvramfs::lseek(fd, MIN_ALLOC_SZ, SEEK_SET);
  if (seek == -1) {
    perror("lseek failed");
  }
  ASSERT_TRUE(seek != -1);

  void *addr = nvsl::libvramfs::mmap(nullptr, MIN_ALLOC_SZ,
                                     PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (addr == MAP_FAILED) {
    perror("mmap failed");
  }

  ASSERT_TRUE(addr != MAP_FAILED);
  memset(addr, 0, MIN_ALLOC_SZ);
}

TEST(libvramfs, openmultiple) {
  int fd_b = nvsl::libvramfs::open("/mnt/pmem0/b", O_CREAT | O_RDWR);
  int fd_c = nvsl::libvramfs::open("/mnt/pmem0/c", O_CREAT | O_RDWR);

  ASSERT_TRUE(fd_b != -1);
  ASSERT_TRUE(fd_c != -1);
  ASSERT_NE(fd_b, fd_c);

  off_t seek = nvsl::libvramfs::lseek(fd_b, MIN_ALLOC_SZ, SEEK_SET);
  if (seek == -1) {
    perror("lseek failed");
  }
  ASSERT_TRUE(seek != -1);

  seek = nvsl::libvramfs::lseek(fd_c, MIN_ALLOC_SZ, SEEK_SET);
  if (seek == -1) {
    perror("lseek failed");
  }
  ASSERT_TRUE(seek != -1);

  void *addr_b = nvsl::libvramfs::mmap(
      nullptr, MIN_ALLOC_SZ, PROT_READ | PROT_WRITE, MAP_SHARED, fd_b, 0);
  if (addr_b == MAP_FAILED) {
    perror("mmap failed");
  }

  ASSERT_TRUE(addr_b != MAP_FAILED);

  void *addr_c = nvsl::libvramfs::mmap(
      nullptr, MIN_ALLOC_SZ, PROT_READ | PROT_WRITE, MAP_SHARED, fd_c, 0);
  if (addr_c == MAP_FAILED) {
    perror("mmap failed");
  }

  ASSERT_TRUE(addr_c != MAP_FAILED);
  memset(addr_c, 0, MIN_ALLOC_SZ);

  ASSERT_NE(addr_b, addr_c);

  // Memset both regions and check if the allocator accidentaly returned
  // overlapping regions
  memset(addr_b, 'b', MIN_ALLOC_SZ);
  memset(addr_c, 'c', MIN_ALLOC_SZ);

  auto addr_b_bptr = nvsl::RCast<char *>(addr_b);
  auto addr_c_bptr = nvsl::RCast<char *>(addr_c);
  for (size_t i = 0; i < MIN_ALLOC_SZ; i++) {
    ASSERT_EQ(addr_b_bptr[i], 'b');
    ASSERT_EQ(addr_c_bptr[i], 'c');
  }
}
