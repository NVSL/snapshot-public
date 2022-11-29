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

#include "libcxlfs/pfmonitor.hh"
#include "nvsl/common.hh"
#include "nvsl/error.hh"
#include "nvsl/stats.hh"
#include "nvsl/utils.hh"

using nvsl::RCast;
using addr_t = PFMonitor::addr_t;

addr_t Controller::get_available_page() {
  const auto page_idx = (RCast<uint64_t>(rand()) % SHM_PAGE_COUNT);
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

PFMonitor::Callback Controller::handle_fault(addr_t addr,
                                             std::bind(&PFMonitor::Callback)) {
  NVSL_ASSERT(used_pages <= MAX_PAGE_COUNT, "used_pages exceed MAX_PAGE_COUNT");

  int rc = 0;

  if (used_pages == MAX_PAGE_COUNT) {
    rc = evict_a_page(RCast<uint64_t>(shm_start), RCast<uint64_t>(shm_end));
    if (rc == -1) {
      DBGE << "Page eviction failed while handling fault for address "
           << (void *)addr << std::endl;
    }
  }

  /* WARN: Only works in single threaded model, handling multiple page faults
   * will require locks/atomics */
  NVSL_ASSERT(used_pages <= MAX_PAGE_COUNT - 1,
              "used_pages exceed MAX_PAGE_COUNT");

  map_page_from_blkdev(addr);

  return nullptr;
}

int Controller::map_page_from_blkdev(addr_t pf_addr) {
  char *buf = RCast<char *>(malloc(page_size));

  auto start_lba = (pf_addr >> 12) * 8;

  ubd->read_blocking(buf, start_lba, 8);

  const auto map_addr_pt = RCast<uint64_t *>(pf_addr);
  memcpy(map_addr_pt, buf, page_size);
  return 1;
}

/** @brief Initialize the internal state **/
int Controller::init() {
  int rc = 0;

  ubd = new UserBlkDev;
  pfm = new PFMonitor;
  mbd = new MemBWDist;

  page_size = RCast<size_t>(sysconf(_SC_PAGESIZE));
  shm_size = MAX_PAGE_COUNT * page_size;

  ubd->init();
  pfm->init();

  const auto prot = PROT_READ | PROT_WRITE;
  const auto flags = MAP_ANONYMOUS | MAP_PRIVATE;
  auto shm_addr = mmap(nullptr, shm_size, prot, flags, -1, 0);

  if (shm_addr == MAP_FAILED) {
    DBGE << "mmap call for allocating shared memory failed" << std::endl;
    return -1;
  }

  shm_start = RCast<char *>(shm_addr);

  rc = pfm->register_range(shm_addr, shm_size);

  if (rc == -1) {
    DBGE << "Failed to register the range for the page fault handler"
         << std::endl;
    return -1;
  }

  rc = pfm->monitor(handle_fault);
  if (rc == -1) {
    DBGE << "Unable to monitor for page fault" << std::endl;
    return -1;
  }

  return 0;
}
