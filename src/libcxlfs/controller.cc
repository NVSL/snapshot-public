// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   pfcontroller.cc
 * @date   novembre  16, 2022
 * @brief  Controller to deal with page fault
 */

#include <functional>
#include <sys/mman.h>
#include <unistd.h>

#include "libcxlfs/controller.hh"
#include <pthread.h>

#include "libcxlfs/pfmonitor.hh"
#include "nvsl/common.hh"
#include "nvsl/error.hh"
#include "nvsl/stats.hh"
#include "nvsl/utils.hh"

using nvsl::RCast;
using addr_t = PFMonitor::addr_t;

addr_t Controller::get_available_page() {
  const auto page_idx = RCast<uint64_t>(rand() % SHM_PAGE_COUNT);
  const auto addr = (shm_start + (page_idx << 12));

  return RCast<addr_t>(addr);
}

int Controller::evict_a_page(addr_t start_addr, addr_t end_addr) {
  const auto dist = mbd->get_dist(start_addr, end_addr);

  addr_t target_page = -1;

  for (uint64_t i = 0; i < SHM_PAGE_COUNT; i++) {
    if (dist[i]) {
      target_page = i;
      break;
    }
  }

  (void)target_page;

  return 0;
}

int Controller::map_page_from_blkdev(addr_t pf_addr) {
  char *buf = RCast<char *>(malloc(page_size));

  const auto addr_off = RCast<uint64_t>(pf_addr) - RCast<uint64_t>(shm_start);
  const auto start_lba = (addr_off >> 12) * 8;

  ubd->read_blocking(buf, start_lba, 8);

  const auto prot = PROT_READ | PROT_WRITE;
  const auto flag = MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE;

  void *mmap_addr = mmap((void *)pf_addr, page_size, prot, flag, -1, 0);

  if ((mmap_addr == MAP_FAILED) and (mmap_addr == (void *)pf_addr)) {
    DBGE << "mmap for handling page fault failed. Reason: " << PSTR()
         << std::endl;
    exit(1);
  }

  DBGH(2) << "Memcpying the page from the blkdev to memory\n";
  memcpy((void *)pf_addr, buf, page_size);

  return 1;
}

void Controller::monitor_thread() {
  PFMonitor::Callback notify_page_fault = [&](PFMonitor::addr_t addr) {
    NVSL_ASSERT(used_pages <= MAX_PAGE_COUNT,
                "used_pages exceed MAX_PAGE_COUNT");

    DBGH(2) << "Replacing page " << RCast<void *>(addr) << "\n";

    if (used_pages == SHM_PAGE_COUNT) {
      DBGH(2) << "Replacing page: start evict " << RCast<void *>(addr) << "\n";
      evict_a_page(RCast<uint64_t>(RCast<char *>(shm_start)),
                   RCast<uint64_t>(shm_end));
    } else {
      DBGH(2) << "Replacing page: not ran out of page " << RCast<void *>(addr)
              << "\n";
    }

    map_page_from_blkdev(addr);
    DBGH(2) << "Replacing page done " << RCast<void *>(addr) << "\n";
  };
  pfm->monitor(notify_page_fault);
  return;
}

void Controller::write_to_ubd(void *buf, char *addr) {
  const auto addr_off = RCast<uint64_t>(addr) - RCast<uint64_t>(shm_start);
  const auto addr_page = (addr_off >> 12);
  const auto start_lba = addr_page * 8;

  DBGH(2) << "Write to " << RCast<void *>(addr) << "\n";

  ubd->write_blocking(buf, start_lba, 8);
  return;
}

void Controller::read_from_ubd(void *buf, char *addr) {
  const auto addr_off = RCast<uint64_t>(addr) - RCast<uint64_t>(shm_start);
  const auto addr_page = (addr_off >> 12);
  const auto start_lba = addr_page * 8;

  DBGH(2) << "Read from " << RCast<uint64_t>(addr) << "\n";

  ubd->read_blocking(buf, start_lba, 8);
  return;
}

/** @brief Initialize the internal state **/
int Controller::init() {
  int rc = 0;

  ubd = new UserBlkDev;
  pfm = new PFMonitor;
  mbd = new MemBWDist;

  page_size = (uint64_t)sysconf(_SC_PAGESIZE);
  shm_size = MAX_PAGE_COUNT * page_size;

  ubd->init();
  pfm->init();

  const auto prot = PROT_READ | PROT_WRITE;
  const auto flags = MAP_ANONYMOUS | MAP_PRIVATE;
  auto shm_addr = mmap(nullptr, shm_size, prot, flags, -1, 0);

  if (shm_addr == MAP_FAILED) {
    DBGE << "mmap call for allocating shared memory failed" << std::endl;
    DBGE << PSTR() << std::endl;
    return -1;
  }

  shm_start = RCast<char *>(shm_addr);

  rc = pfm->register_range(shm_addr, shm_size);

  if (rc == -1) {
    DBGE << "Failed to register the range for the page fault handler"
         << std::endl;
    return -1;
  }

  monitor_thread();

  if (rc == -1) {
    DBGE << "Unable to monitor for page fault" << std::endl;
    return -1;
  }

  return 0;
}

void *Controller::get_shm() {
  return shm_start;
}

uint64_t Controller::get_shm_len() {
  return SHM_PAGE_COUNT << 12;
}
