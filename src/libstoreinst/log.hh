// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   log.hh
 * @date   mars 25, 2022
 * @brief  Brief description here
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <numeric>

#include "immintrin.h"
#include "libc_wrappers.hh"
#include "libstoreinst.hh"
#include "nvsl/common.hh"
#include "nvsl/stats.hh"

namespace fs = std::filesystem;

namespace nvsl {
  namespace cxlbuf {
    /**
     * @brief struct for storing log in persistent memory
     */
    struct log_t {
      uint64_t addr;
      uint64_t bytes;

      NVSL_BEGIN_IGNORE_WPEDANTIC
      uint64_t val[];
      NVSL_END_IGNORE_WPEDANTIC
    };

    /**
     * @brief lean struct for tracking logged address and size
     */
    struct log_lean_t {
      uint64_t addr;
      uint64_t bytes;
    };

    class Log {
    private:
      static constexpr const char* LOG_LOC = "/mnt/pmem0/cxlbuf_logs/";
      void init_dirs();

      /** @brief initialize and map this thread's log buffer */
      void init_thread_buf();
    public:
      static constexpr const size_t MAX_ENTRIES = 1024;
      static constexpr const size_t BUF_SZ = 128 * LP_SZ::MiB;

      /** @brief Voltile list of all the entries */
      std::vector<log_lean_t> entries;

      size_t current_log_off;
      
      /** @brief Persistent log area */
      char *log_area = nullptr;

      Log();

      void log_range(void *start, size_t bytes);
    };

    extern nvsl::Counter *skip_check_count, *logged_check_count;
    extern nvsl::StatsFreq<> *tx_log_count_dist;
  }
}

extern thread_local nvsl::cxlbuf::Log tls_log;
