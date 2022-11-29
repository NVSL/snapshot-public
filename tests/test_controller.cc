// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   test_controller.cc
 * @date   novembre  21, 2022
 * @brief  Brief description here
 */

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

void *fault_page_ctl(void *args) {
  char *addr = nvsl::RCast<char *>(args);

  *addr = 0xf;

  return nullptr;
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

  ASSERT_EQ(memcmp(buf2, buf, 0x1000), 0);
}

TEST(controller, fault_page_ctl) {

  fault_page_ctl(test_page_ctr);

  ASSERT_EQ(*test_page_ctr, 0xf);
}
