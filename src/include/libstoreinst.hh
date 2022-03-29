// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   libstoreinst.hh
 * @date   mars 14, 2022
 * @brief  Brief description here
 */

#pragma once

#include <unistd.h>

#define BUF_SIZE (100 * 1000 * 4096)

namespace nvsl {
  class PMemOps;
}

extern "C" {
  void *memcpy(void *__restrict dst, const void *__restrict src, size_t n)
    __THROW;

  void *memmove(void *__restrict dst, const void *__restrict src, size_t n)
    __THROW;

  extern bool startTracking;
  int snapshot(void *addr, size_t length, int flags);

  void libstoreinst_ctor();

  extern void *start_addr, *end_addr;
  extern char *log_area, *pm_back;
  extern size_t current_log_off;
  extern bool startTracking;
  extern bool storeInstEnabled;
  extern bool cxlModeEnabled;
  extern nvsl::PMemOps *pmemops;
}
