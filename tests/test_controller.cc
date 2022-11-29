// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   test_controller.cc
 * @date   novembre  21, 2022
 * @brief  Brief description here
 */

#include "nvsl/clock.hh"
#include "gtest/gtest.h"
#include <cstdlib>
#include <iostream>
#include <pthread.h>
#include <sys/mman.h>

#include "libcxlfs/controller.hh"
#include "nvsl/common.hh"
#include "nvsl/utils.hh"

using nvsl::RCast;

Controller *ctlor;
char *test_page_ctr;

void fault_page_ctl(char *addr, size_t pg_idx) {
  auto tgt_addr = addr + (pg_idx << 12);

  *tgt_addr = 0xf;
}

TEST(controller, init_test) {
  ctlor = new Controller();

  ctlor->init();

  test_page_ctr = RCast<char *>(ctlor->get_shm());
}

TEST(controller, write_and_read) {
  char *buf = (char *)malloc(4096);
  memset(buf, 0, 0x1000);
  *buf = 0xa;

  ctlor->write_to_ubd(buf, test_page_ctr);

  char *buf2 = (char *)malloc(4096);
  memset(buf2, 0, 0x1000);
  ctlor->read_from_ubd(buf2, test_page_ctr);

  ASSERT_EQ(*buf2, *buf);
}

TEST(controller, fault_page_ctl) {
  nvsl::Clock clk;
  clk.tick();
  for (int i = 0; i < 4; i++) {
    fault_page_ctl(test_page_ctr, i);
  }
  clk.tock();

  std::cerr << "Average page fault time = " << clk.ns() / 4000 << "us \n";

  ASSERT_EQ(*test_page_ctr, 0xf);
}
