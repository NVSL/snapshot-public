// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   bgflush.hh
 * @date   avril  5, 2022
 * @brief  Brief description here
 */

#pragma once

#include <unistd.h>
#include <cstdint>

namespace nvsl {
  namespace cxlbuf {
    namespace bgflush {

      constexpr size_t BGF_JOB_QUEUE_MAX_LEN = 128;

      struct bgf_job_t {
        void *addr;
        size_t bytes;
      };

      void push(const bgf_job_t entry);

      void drain();
      
      void launch();

      void clear();
    }
  }
} // namespace nvsl
  
