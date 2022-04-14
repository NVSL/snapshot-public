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
        uint64_t bytes : 24;
        uint64_t addr : 56;

        NVSL_BEGIN_IGNORE_WPEDANTIC
        uint64_t content[];
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
        EMPTY = 0,
        ACTIVE = 1,
        COMMITTED = 2,
      };

      struct log_entry_iter;

      struct log_layout_t {
        State state;
        uint64_t log_offset;

        NVSL_BEGIN_IGNORE_WPEDANTIC
        log_entry_t content[];
        NVSL_END_IGNORE_WPEDANTIC

        /*-- Iterators --*/
        log_entry_iter begin() const {
          return this->cbegin();
        }

        log_entry_iter end() const {
          return this->cend();
        }

        const log_entry_iter cbegin() const {
          auto *result = this->content;

          DBGH(4) << "log_entry_iter from begin = " << (void*)(result)
                  << std::endl;

          return log_entry_iter(result);
        }

        const log_entry_iter cend() const {
          auto *byte_ptr = RCast<const uint8_t *>(this->content);
          byte_ptr += this->log_offset;

          auto *last_entry = RCast<const log_entry_t*>(byte_ptr);

          return log_entry_iter(last_entry);
        }
      };

      /**
       * @brief Log entry iterator
       */
      struct log_entry_iter {
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = log_entry_t;
        using pointer = const log_entry_t *;   // or also value_type*
        using reference = const log_entry_t &; // or also value_type&

        friend bool operator==(const log_entry_iter &a, const log_entry_iter &b) {
          return a.entry == b.entry;
        }

        friend bool operator!=(const log_entry_iter &a, const log_entry_iter &b) {
          return a.entry != b.entry;
        }

        log_entry_iter(const pointer entry) : entry(entry) {
          DBGH(4) << "Creating a new log entry iter for entry with address "
                  << (void*)(0UL + entry->addr) << " and content: \n"
                  << buf_to_hexstr((char *)entry->content, entry->bytes) << "\n";
        }

        reference operator*() const { return *entry; }

        pointer operator->() { return entry; }

        log_entry_iter &operator++() {
          auto old_val = this->entry;

          auto new_ptr_ul = (size_t)(entry) + entry->bytes + sizeof(*entry);
          this->entry = RCast<pointer>(new_ptr_ul);
          // this->entry = (pointer)align_cl(RCast<pointer>(new_ptr_ul));

          DBGH(4) << "Log entry incrementing from " << (void*)(old_val) << " to "
                  << this->entry << std::endl;

          return *this;
        }

        log_entry_iter operator++(int) {
          log_entry_iter result = *this;

          auto new_ptr_ul = (uint64_t)entry + entry->bytes;
          this->entry = RCast<pointer>(new_ptr_ul);

          return result;
        }

      private:
        pointer entry;
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

#ifdef LOG_FORMAT_VOLATILE
      /** @brief Voltile list of all the entries */
      std::vector<log_entry_lean_t> entries;
#endif

      void clear() {
        log_area->log_offset = 0;
        last_flush_offset = 0;

#ifdef LOG_FORMAT_VOLATILE
        entries.clear();
#endif

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

        DBGH(3) << "Updating log state to " << state << std::endl;

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
extern int trace_fd;
