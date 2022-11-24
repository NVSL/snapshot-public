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

Controller *ctlor;
char *test_page_ctr;

void *fault_page_ctl(void *args) {
  char *addr = nvsl::RCast<char *>(args);
  *addr = 0xf;

	

  return nullptr;
}

void *init_test(void *){
	return nullptr;
}

TEST(init_test, controller) {
	ctlor = new Controller();
  ctlor->init();

	test_page_ctr = ctlor->getSharedMemAddr();
}



TEST(fault_page_ctl, controller) {
  // pthread_t thr_ctr;

  // pthread_create(&thr_ctr, nullptr, init_test, nullptr);
	
  fault_page_ctl(test_page_ctr);
	
  ASSERT_NE(*test_page_ctr, 0xf);
}




