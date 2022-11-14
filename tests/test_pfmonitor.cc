// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   test_blkdev.cc
 * @date   novembre  9, 2022
 * @brief  Brief description here
 */

#include "gtest/gtest.h"
#include <cstdlib>
#include <iostream>
#include <pthread.h>
#include <sys/mman.h>

#include "libcxlfs/pfmonitor.hh"
#include "nvsl/common.hh"
#include "nvsl/utils.hh"

PFMonitor *pfm;
char *test_page;

void *fault_page(void *args) {
  char *addr = nvsl::RCast<char *>(args);
  *addr = 0xf;

  return nullptr;
}

void *monitor_thread(void *) {
  PFMonitor::Callback cb = [](PFMonitor::addr_t addr) {
    // int rc =
    //     mprotect(nvsl::RCast<void *>(addr), 0x1000, PROT_READ | PROT_WRITE);
    mmap((void *)addr, 0x1000, PROT_READ | PROT_WRITE,
         MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    // if (rc != 0) {
    //   DBGE << "mprotect: " << PSTR() << "\n";
    // }
    DBGH(0) << "Got a page fault for address " << addr << std::endl;
  };

  pfm->monitor(cb);

  return nullptr;
}

TEST(init, pfmonitor) {
  pfm = new PFMonitor();
  pfm->init();
}

TEST(register_range, pfmonitor) {
  void *raw_addr = mmap(nullptr, 0x1000, PROT_READ | PROT_WRITE,
                        MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

  if (raw_addr == (void *)-1) {
    DBGE << "MMAP failed\n";
    FAIL();
  }

  test_page = nvsl::RCast<char *>(raw_addr);

  int rc = pfm->register_range(test_page, 0x1000);
  if (rc != 0) {
    perror("pfm->register_range");
  }

  ASSERT_EQ(rc, 0);
}

TEST(fault_page, pfmonitor) {
  pthread_t thr;

  pthread_create(&thr, nullptr, monitor_thread, nullptr);

  fault_page(test_page);

  ASSERT_EQ(*test_page, 0xf);
}
