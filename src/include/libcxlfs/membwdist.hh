// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   membwdist.hh
 * @date   novembre 14, 2022
 * @brief  Brief description here
 */

#pragma once

#include <cstdint>
#include <linux/perf_event.h>
#include <stddef.h>

#include <array>
#include <tuple>
#include <unordered_map>
#include <vector>

class MemBWDist {
public:
  using addr_t = uint64_t;
  using ip_t = uint64_t;
  using record_t = std::pair<ip_t, addr_t>;
  using heat_t = bool;
  using dist_t = std::unordered_map<addr_t, heat_t>;

private:
  struct pebs_sample_t {
    struct perf_event_header header;
    __u64 ip;   /* if PERF_SAMPLE_IP */
    __u64 addr; /* if PERF_SAMPLE_ADDR */
  };

  constexpr static size_t BUF_SZ_PG_CNT = 0x100;

  /** @brief PEBS sample buffer */
  perf_event_mmap_page *sample_buf;

public:
  MemBWDist() {}
  int start_sampling(size_t sampling_freq);
  std::vector<MemBWDist::record_t> get_samples();
  dist_t get_dist(addr_t start, addr_t end);
};
