// -*- mode: c++; c-basic-offset: 2; -*-

#pragma once

/**
 * @file   clock.hh
 * @date   novembre 10, 2021
 * @brief  Clock primitives for benchmarking workloads
 */

#ifndef LP_ASSERT
#include <cassert>
#endif

#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace nvsl {
  inline std::string ns_to_hr_clk(const size_t ns_total) {
    std::stringstream ss;

    const size_t s = ns_total / (1000000000);
    const size_t ms = (ns_total - s * 1000000000) / (1000000);
    const size_t us = (ns_total - s * 1000000000 - ms * 1000000) / (1000);
    const size_t ns =
        (ns_total - s * 1000000000 - ms * 1000000 - us * 1000);

    ss << s << "s " << ms << "ms " << us << "us " << ns << "ns";

    return ss.str();
  }

  /** @brief Clock object based on std::chrono::high_resolution_clock */
  class Clock {
  private:
    std::chrono::system_clock::time_point start_clk;
    bool running = false;
    size_t total_ns = 0;
    std::vector<size_t> raw_values;
    std::vector<size_t> sorted_raw_values;

    /**< Total number of values to store */
    static constexpr size_t RAW_VAL_CNT = 1024*1024*100;
  public:
    Clock() {
      raw_values.reserve(RAW_VAL_CNT);
    }
    
    /** @brief Start the timer */
    void tick() {
      running = true;
      this->start_clk = std::chrono::high_resolution_clock::now();
    }

    /** @brief Stop the timer */
    void tock() {
      using namespace std::chrono;

      const auto end_clk = high_resolution_clock::now();
      if (!running) {
#if defined(LP_ERROR) && defined(DBGE)
        LP_ERROR("Clock not running");
#else
        std::cerr << "Clock not running" << std::endl;
        exit(1);
#endif
      }

      running = false;
      const auto elapsed = duration_cast<nanoseconds>(end_clk - start_clk).count();
      this->total_ns += elapsed;

#define OVERFLOW_MSG "Raw values buffer overflow, increase array size or " \
        "operations/loop"
#if defined(LP_ASSERT) && defined(DBGE)      
      LP_ASSERT(this->raw_values.size() < RAW_VAL_CNT, OVERFLOW_MSG);
#else
      if (this->raw_values.size() > RAW_VAL_CNT) {
        fprintf(stderr, OVERFLOW_MSG "\n");
        exit(1);
      }
#endif
      this->raw_values.push_back(elapsed);
    }

    void reset() {
      this->running = false;
      this->total_ns = 0;
    }

    size_t ns() const { return this->total_ns; }

    /** @brief Total time elapsed */
    const std::string summarize() const {
      std::stringstream ss;

      size_t ns_total = this->ns();

      ss << "Total ns:   " << this->ns() << std::endl;
      ss << "Total time: " << ns_to_hr_clk(ns_total);

      return ss.str();
    }

    /**
     * @brief Prepare clock object to calculate percentile/summary
     * @details non-const function to update the internal state so that other
     * functions can be const
     */
    void reconcile() {
      sorted_raw_values.reserve(raw_values.size());
      std::copy(raw_values.begin(), raw_values.end(),
                std::back_inserter(sorted_raw_values));
      std::sort(sorted_raw_values.begin(), sorted_raw_values.end());
    }

    /**
     * @brief Calculate the percentile value
     * @param[in] pc Percentile out of 100
     */
    size_t percentile(const size_t pc) const {
      if (sorted_raw_values.size() == 0) {
#if defined(LP_ERROR)
        LP_ERROR("Clock not reconcile. Call reconcile()");
#else
        assert(0 && "Clock not reconciled. Call reconcile()");
#endif
      }
      
      const auto sz = sorted_raw_values.size();
      const auto idx = std::max(0UL, (size_t)((sz*pc)/100.0)-1);
      
      return sorted_raw_values[idx];
    }

    /**
     * @brief Summary with operations per second
     * @param[in] total_ops Total number of operations performed
     * @param[in] distribution Generate distribution in summary
     */
    std::string summarize(size_t total_ops, bool distribution = false) const {
      std::stringstream ss;
      size_t ops_per_iter = total_ops/raw_values.size();
      ss << this->summarize() << std::endl
         << "ops/s: " << (total_ops * (1000000000)) / ((double)this->ns())
         << "\n"
         << "time/op: "
         << ns_to_hr_clk((size_t)(this->ns() / (double)total_ops))
         << "\np50/op: "
         << ns_to_hr_clk((size_t)(this->percentile(50))/ops_per_iter)
         << "\np90/op: "
         << ns_to_hr_clk((size_t)(this->percentile(90))/ops_per_iter)
         << "\np99/op: "
         << ns_to_hr_clk((size_t)(this->percentile(99))/ops_per_iter)
         << "\ntime/op: "
         << ns_to_hr_clk((size_t)(this->ns() / (double)total_ops));

      return ss.str();
    }

    size_t ns_per_op(size_t total_ops) const { return this->ns() / total_ops; }

    size_t percentile_per_op(const size_t total_ops, const size_t pc) const {
      const auto ops_per_iter = total_ops / raw_values.size();
      return this->percentile(pc)/ops_per_iter;
    }
  };
} // namespace nvsl
