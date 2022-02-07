// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   dummy.cc
 * @date   février  6, 2022
 * @brief  Brief description here
 */

#include "libdsaemu.hh"

#include <cassert>
#include <cstring>
#include <iostream>
#include <unistd.h>

const size_t ARR_SZ = 1024;

int main() {
  char arr1[ARR_SZ], arr2[ARR_SZ];

  std::memset(arr1, '#', ARR_SZ * sizeof(arr1[0]));

  dsa::jobdesc_t job;
  job.src = (uint64_t)arr1;
  job.dst = (uint64_t)arr2;
  job.bytes = ARR_SZ * sizeof(arr1[0]);

  std::cout << "Inserted a new job" << std::endl;
  dsa::queue[dsa::queue_head++] = job;

  std::cout << "Sleeping..." << std::endl;
  sleep(1);

  std::cout << "Checking arr2" << std::endl;
  std::cout << "arr2[1] = " << arr2[1] << std::endl;
  assert(arr2[1] == '#');

  exit(0);
}
