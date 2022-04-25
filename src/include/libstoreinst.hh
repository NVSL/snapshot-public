// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   libstoreinst.hh
 * @date   mars 14, 2022
 * @brief  Brief description here
 */

#pragma once

#include <string>
#include <unistd.h>

#define BUF_SIZE (100 * 1000 * 4096UL)

namespace nvsl {
  class PMemOps;
  class Clock;
  class Counter;

  namespace cxlbuf {
    extern std::string *log_loc;
  }
} // namespace nvsl

extern "C" {
void *memcpy(void *__restrict dst, const void *__restrict src,
             size_t n) __THROW;

void *memmove(void *__restrict dst, const void *__restrict src,
              size_t n) __THROW;

extern bool startTracking;
int snapshot(void *addr, size_t length, int flags);

void libstoreinst_ctor();

extern void *start_addr, *end_addr;
extern size_t current_log_off, current_log_cnt;
extern bool startTracking;
extern bool storeInstEnabled;
extern bool cxlModeEnabled;
extern nvsl::PMemOps *pmemops;
}

// TODO: Implement a better solution
extern bool firstSnapshot;
extern bool crashOnCommit;
extern bool nopMsync;
extern nvsl::Clock *perst_overhead_clk;
extern size_t msyncSleepNs;
extern nvsl::Counter snapshots;
