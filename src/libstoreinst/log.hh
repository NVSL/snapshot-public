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
#include "nvsl/error.hh"
#include "nvsl/pmemops.hh"
#include "nvsl/stats.hh"

namespace fs = std::filesystem;

namespace nvsl {
  namespace cxlbuf {
    class Log {
    public:
      /**
       * @brief struct for storing log in persistent memory
       */
      struct log_entry_t {
        // uint8_t is_disabled : 8;
        uint64_t bytes : 8;
        uint64_t addr : 56;

        NVSL_BEGIN_IGNORE_WPEDANTIC
        uint64_t val[];
        NVSL_END_IGNORE_WPEDANTIC
      } __attribute__((packed));

      /**
       * @brief lean struct for tracking logged address and size
       */
      struct log_entry_lean_t {
        uint64_t addr;
        uint64_t bytes;
      };

      enum State : uint64_t {
        EMPTY,
        ACTIVE,
        COMMITTED
      };

      struct log_layout_t {
        State state;
        uint64_t log_offset;

        NVSL_BEGIN_IGNORE_WPEDANTIC
        log_entry_t entries[];
        NVSL_END_IGNORE_WPEDANTIC
      };

      static constexpr const char* LOG_LOC = "/mnt/pmem0/cxlbuf_logs/";
    private:
      size_t last_flush_offset = 0;
      
      void init_dirs();

      /** @brief initialize and map this thread's log buffer */
      void init_thread_buf();
    public:
      static constexpr const size_t MAX_ENTRIES = 1024;
      static constexpr const size_t BUF_SZ = 128 * LP_SZ::MiB;

      /** @brief Voltile list of all the entries */
      std::vector<log_entry_lean_t> entries;

      void clear() {
        log_area->log_offset = 0;
        last_flush_offset = 0;
        entries.clear();
      }

      /** @brief Persistent log area */
      log_layout_t *log_area = nullptr;

      /** @brief Get a log_layout_t object using its pid.tid name */
      static std::tuple<Log::log_layout_t*, fs::path>
      get_log_by_id(const std::string &name, void *addr = nullptr);
      
      Log();

      void log_range(void *start, size_t bytes);

      void set_state(State state, bool flush_whole = false) {
        NVSL_ASSERT(this->log_area != nullptr, "Log area not initialized");


        if (flush_whole) {
          this->log_area->state = state;
          pmemops->flush(this->log_area, sizeof(*this->log_area));
        } else {
          pmemops->streaming_wr(&this->log_area->state, &state, 
                                sizeof(this->log_area->state));
        }

        pmemops->drain();

      }

      State get_state() const {
        NVSL_ASSERT(this->log_area != nullptr, "Log area not initialized");

        return this->log_area->state; 
      }

      void flush_all() const;
    };

    extern nvsl::Counter *skip_check_count, *logged_check_count;
    extern nvsl::StatsFreq<> *tx_log_count_dist;
  }
}

extern thread_local nvsl::cxlbuf::Log tls_log;
