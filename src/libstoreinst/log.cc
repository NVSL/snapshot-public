// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   log.cc
 * @date   mars 25, 2022
 * @brief  Brief description here
 */

#include <cassert>

#include "log.hh"
#include "nvsl/stats.hh"

nvsl::Counter *skip_check_count, *logged_check_count;
nvsl::StatsFreq<> *tx_log_count_dist;



void log_range(void *start, size_t bytes) {
  auto &log_entry = *(log_t*)((char*)log_area + current_log_off);
    
  if (log_area != nullptr and cxlModeEnabled) {
    storeInstEnabled = true;
      
    // printf("checking mem %p\n", ptr);
    log_entry.addr = (uint64_t)start;
    log_entry.bytes = bytes;

    real_memcpy(&log_entry.val, start, bytes);

    const auto cl_count = (sizeof(log_t) + bytes+63)/64;
    size_t cur_cl = 0;
    while (cur_cl < cl_count) {
      _mm_clwb((char*)&log_entry + cur_cl*64);
      cur_cl++;
    }
    _mm_sfence();
    ++*logged_check_count;
    current_log_off += sizeof(log_t) + bytes;

    assert(current_log_off < BUF_SIZE);

  }
}
