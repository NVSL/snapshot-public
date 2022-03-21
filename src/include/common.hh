// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   common.hh
 * @date   December 25, 2020
 * @brief  Common macros, constexpr and function definitions
 */

#pragma once

#include "nvsl/envvars.hh"
#include "nvsl/constants.hh"

#include <chrono>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <fnmatch.h>

namespace common {
  inline void bindcore(int cpuid) {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpuid, &mask);
    int ret = sched_setaffinity(0, sizeof(mask), &mask);

    if (ret != 0) {
      perror("sched_setaffinity");
      exit(1);
    } else {
      if (sched_getcpu() != cpuid) {
        std::cerr << "Unable to set CPU affinity" << std::endl;
        exit(1);
      }
    }

  }

} // namespace common
  
  

