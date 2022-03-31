// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   log.hh
 * @date   mars 25, 2022
 * @brief  Brief description here
 */

#pragma once

#include <cstdint>
#include <numeric>

#include "immintrin.h"
#include "libc_wrappers.hh"
#include "libstoreinst.hh"
#include "nvsl/common.hh"
#include "nvsl/stats.hh"

struct log_t {
  uint64_t addr;
  uint64_t bytes;

  NVSL_BEGIN_IGNORE_WPEDANTIC
  uint64_t val[];
  NVSL_END_IGNORE_WPEDANTIC
};

struct log_lean_t {
  uint64_t addr;
  uint64_t bytes;
};

void log_range(void *start, size_t bytes);

extern nvsl::Counter *skip_check_count, *logged_check_count;
extern nvsl::StatsFreq<> *tx_log_count_dist;
