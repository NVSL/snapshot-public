// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   workingsetsize.cc
 * @date   Novembre 30, 2022
 * @brief  Understand the cxlbuffs latency behavior
 */

#include "libcxlfs/controller.hh"
#include "nvsl/clock.hh"
#include "nvsl/constants.hh"
#include "nvsl/envvars.hh"
#include "nvsl/pmemops.hh"
#include "nvsl/utils.hh"
#include "run.hh"

#include "libcxlfs/libcxlfs.hh"

#include <algorithm>
#include <fcntl.h>
#include <iomanip>
#include <map>
#include <pthread.h>
#include <random>
#include <set>
#include <sys/mman.h>

using namespace nvsl;

constexpr size_t CACHE_SIZE = 128 * nvsl::MiB;
constexpr size_t TOTAL_SHM_SIZE = 1 * nvsl::GiB;

constexpr size_t MAX_ACCESSES = 500000;
constexpr size_t MIN_WRK_SET_SIZE = 1;
constexpr size_t MAX_WRK_SET_SIZE = TOTAL_SHM_SIZE;

void mb_workingsetsize() {
  Controller ctrl;
  ctrl.init(CACHE_SIZE >> 12, TOTAL_SHM_SIZE >> 12);

  char *shm = RCast<char *>(ctrl.get_shm());

  for (size_t wss = MIN_WRK_SET_SIZE; wss < MAX_WRK_SET_SIZE; wss <<= 1) {
    Clock clk;
    clk.tick();
    for (size_t access_i = 0; access_i < MAX_ACCESSES; access_i++) {
      shm[rand() % wss] += 1;
    }
    clk.tock();
    std::cerr << wss << ", " << clk.ns() / (MAX_ACCESSES * 1000) << "."
              << std::setw(3) << std::setfill('0')
              << (clk.ns() / MAX_ACCESSES) % 1000 << "us";
    std::cerr << ", " << ctrl.faults.value();
    ctrl.flush_cache();

    std::random_device rd{};
    std::mt19937 gen{rd()};

    std::normal_distribution<> d{wss / 2.0, std::max(wss / 16.0, 1.0)};
    clk.reset();
    clk.tick();
    for (size_t access_i = 0; access_i < MAX_ACCESSES; access_i++) {
      shm[int(d(gen)) % wss] += 1;
    }
    clk.tock();
    std::cerr << ", " << clk.ns() / (MAX_ACCESSES * 1000) << "." << std::setw(3)
              << std::setfill('0') << (clk.ns() / MAX_ACCESSES) % 1000 << "us";

    std::cerr << ", " << ctrl.faults.value() << "\n";
    ctrl.flush_cache();
  }

  // /* Generate results */
  // for (const auto &[k, vs] : results) {
  //   NVSL_ASSERT(vs[0].size() == vs[1].size(),
  //               "Both vectors should be same sized");

  //   size_t drain_dist = DRAIN_DST_START;
  //   for (size_t elem_i = 0; elem_i < vs[0].size(); elem_i++) {
  //     std::cout << k << ", " << drain_dist << ", " << vs[0][elem_i] << ", "
  //               << vs[1][elem_i] << std::endl;
  //     drain_dist *= DRAIN_DST_INCR;
  //   }
  // }
}
