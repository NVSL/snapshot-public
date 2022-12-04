// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   controller.hh
 * @date   novembre 16, 2022
 * @brief  Brief description here
 */
#pragma once
#include <functional>
#include <memory>
#include <pthread.h>

#include "libcxlfs/blkdev.hh"
#include "libcxlfs/membwdist.hh"
#include "libcxlfs/numabinder.hh"
#include "libcxlfs/pfmonitor.hh"
#include "nvsl/clock.hh"
#include "nvsl/stats.hh"

class Controller {
public:
  nvsl::Counter faults;

private:
  using addr_t = PFMonitor::addr_t;

  static constexpr size_t REMOTE_NODE = 0;

  std::size_t max_active_pg_cnt = 2;
  std::size_t shm_pg_cnt = (64 * 1024UL);

  UserBlkDev *ubd = nullptr;
  PFMonitor *pfm = nullptr;
  MemBWDist *mbd = nullptr;
  NumaBinder *nbd = nullptr;

  nvsl::Clock blk_rd_clk;

  uint64_t page_size, shm_size;
  std::unordered_map<addr_t, bool> mapped_pages;
  std::vector<std::unique_ptr<UserBlkDev::ubd_sequence>> ubd_wr_cq;

  /** @brief Tracks the pages currently mapped in the SHM **/
  uint64_t used_pages = 0;

  char *shm_start;
  char *shm_end;

  bool monitor_thread_running = false;

  /** @brief Called when the SHM receives a page fault **/
  PFMonitor::Callback handle_fault(addr_t addr);
  addr_t get_available_page();

  int evict_a_page();
  void *map_page_from_blkdev(addr_t pf_addr);

  static void *monitor_thread_wrapper(void *obj) {
    reinterpret_cast<Controller *>(obj)->monitor_thread();
    return 0;
  }

  void monitor_thread();
  void write_to_ubd(void *buf, char *addr);
  void read_from_ubd(void *buf, char *addr);

public:
  Controller();

  /** @brief Initialize the internal state **/
  int init(std::size_t max_active_pg_cnt = 2,
           std::size_t shm_pg_cnt = (64 * 1024UL));

  void *get_shm();
  uint64_t get_shm_len();
  void *get_shm_end();

  void flush_cache();
  void resize_cache(size_t pg_cnt);
  void reset_stats();
};
