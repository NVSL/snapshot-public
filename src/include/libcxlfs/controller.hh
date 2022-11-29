// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   controller.hh
 * @date   novembre 16, 2022
 * @brief  Brief description here
 */
#pragma once
#include <functional>
#include <pthread.h>

#include "libcxlfs/blkdev.hh"
#include "libcxlfs/membwdist.hh"
#include "libcxlfs/pfmonitor.hh"

class Controller {
public:
private:
  using addr_t = PFMonitor::addr_t;

  UserBlkDev *ubd;
  PFMonitor *pfm;
  MemBWDist *mbd;

  static constexpr uint64_t MAX_PAGE_COUNT = 2;
  static constexpr uint64_t SHM_PAGE_COUNT = 16384;

  uint64_t page_size, shm_size;

  /** @brief Tracks the pages currently mapped in the SHM **/
  uint64_t used_pages = 0;

  char *shm_start;
  char *shm_end;

  /** @brief Called when the SHM receives a page fault **/
  PFMonitor::Callback handle_fault(addr_t addr);
  addr_t get_available_page();

  int evict_a_page(addr_t start, addr_t end);
  int map_page_from_blkdev(addr_t pf_addr);

  static void *monitor_thread_wrapper(void *obj) {
    reinterpret_cast<Controller *>(obj)->monitor_thread();
    return 0;
  }

  void monitor_thread();

public:
  Controller() {}

  /** @brief Initialize the internal state **/
  int init();
  void write_to_ubd(void *buf, char *addr);
  void read_from_ubd(void *buf, char *addr);

  void *get_shm();
  uint64_t get_shm_len();
};
