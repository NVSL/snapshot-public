// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   log.cc
 * @date   mars 25, 2022
 * @brief  Brief description here
 */

#include <cassert>

#include "log.hh"
#include "nvsl/stats.hh"
#include "nvsl/pmemops.hh"
// #include "libdsaemu.hh"

nvsl::Counter *skip_check_count, *logged_check_count;
nvsl::StatsFreq<> *tx_log_count_dist;

extern nvsl::PMemOps *pmemops;

void log_range(void *start, size_t bytes) {
  auto &log_entry = *(log_t*)((char*)log_area + current_log_off);
  auto &addr_entry = *((log_lean_t*)addr_area + current_log_cnt);
    
  if (log_area != nullptr and cxlModeEnabled) {
    storeInstEnabled = true;

    /* Update the volatile address list */
    addr_entry.addr = (uint64_t)start;
    addr_entry.bytes = bytes;
    
    /* Write to the persistent log and flush and fence it */
    log_entry.addr = (uint64_t)start;
    log_entry.bytes = bytes;
    real_memcpy(&log_entry.val, start, bytes);
    pmemops->flush(&log_entry, sizeof(log_entry) + bytes);

    ++*logged_check_count;

    /* Align the log offset to the next cacheline. This reduces the number of
       clwb needed */
    current_log_off += sizeof(log_t) + bytes;
    current_log_off = ((current_log_off+63)/64)*64;

    current_log_cnt++;

    assert(current_log_off < BUF_SIZE);
  }
}
