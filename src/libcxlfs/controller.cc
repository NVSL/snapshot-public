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

using nvsl::P;
using nvsl::RCast;
using addr_t = PFMonitor::addr_t;

static unsigned int g_seed;

// Compute a pseudorandom integer.
// Output value in range [0, 32767]
inline int fast_rand(void) {
  g_seed = (214013 * g_seed + 2531011);
  return (g_seed >> 16) & 0x7FFF;
}

addr_t Controller::get_available_page() {
  const auto page_idx = RCast<uint64_t>(fast_rand() % shm_pg_cnt);
  const auto addr = (shm_start + (page_idx << 12));

  return RCast<addr_t>(addr);
}

int Controller::evict_a_page() {
  const auto rg_start = RCast<addr_t>(get_shm());
  const auto rg_end = RCast<addr_t>(get_shm_end());

  const auto dist = mbd->get_dist(rg_start, rg_end);

  addr_t target_page_idx = -1;

  for (const auto [pg_idx, _] : mapped_pages) {
    if (!dist.contains(pg_idx)) {
      target_page_idx = pg_idx;
    }
  }

  if (target_page_idx == (addr_t)-1) {
    DBGH(1) << "Unable to find a page to evict from the distribution\n";

    auto mp_iter = mapped_pages.begin();
    std::advance(mp_iter, rand() % mapped_pages.size());

    target_page_idx = (*mp_iter).first;

    DBGH(1) << "Using random page " << target_page_idx << std::endl;
  }

  DBGH(2) << "Found a page to evict " << target_page_idx << "\n";

  DBGH(3) << "Removing page from the mapped page list" << std::endl;
  const auto target_page = P(shm_start + (target_page_idx << 12));

  // Write the page to the backing media
  // TODO: Check if the page is actually dirty
  auto ubd_seq = ubd->write(target_page, target_page_idx * 8, page_size >> 9);
  if (ubd_seq) {
    ubd_wr_cq.push_back(std::move(ubd_seq));
    DBGH(4) << "Added a new entry to the write completion queue. New size "
            << ubd_wr_cq.size() << "\n";
  } else {
    DBGE << "Page write back failed\n";
    return -1;
  }

  for (auto iter = ubd_wr_cq.size() - 1; iter > 0; iter--) {
    auto &entry = ubd_wr_cq[iter];
    if (entry->is_completed) {
      entry->free();
      ubd_wr_cq.erase(ubd_wr_cq.begin() + iter);
      DBGH(4) << "Deleting completed entry from the write completion queue "
              << iter << "\n";
    } else {
      DBGH(4) << "Write request idx " << iter << " not yet done\n";
    }
  }

  const auto prot = PROT_READ | PROT_WRITE;
  const auto flags = MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED;
  auto addr = mmap(target_page, page_size, prot, flags, -1, 0);
  pfm->register_range(target_page, page_size);

  if (addr == MAP_FAILED) {
    DBGE << "mmap() for evicted page failed: " << PSTR() << std::endl;
    return -1;
  }

  if (addr != target_page) {
    DBGE << "Unable to remap at the same address" << std::endl;
    return -1;
  }

  mapped_pages.erase(target_page_idx);
  used_pages--;

  return 0;
}

void *Controller::map_page_from_blkdev(addr_t pf_addr) {
  void *buf = malloc(page_size);
  pf_addr = (pf_addr >> 12) << 12;

  const auto addr_off = RCast<uint64_t>(pf_addr) - RCast<uint64_t>(shm_start);
  const auto start_lba = (addr_off >> 12) * 8;

  DBGH(3) << "Getting lba " << start_lba << std::endl;

  blk_rd_clk.tick();
  ubd->read_blocking(buf, start_lba, 8);
  blk_rd_clk.tock();

  DBGH(3) << "Adding the page to the mapped page list" << std::endl;
  mapped_pages[((pf_addr - RCast<uint64_t>(get_shm())) >> 12)] = true;

  return buf;
}

void Controller::monitor_thread() {
  nbd->bind_to_node(1);
  PFMonitor::Callback notify_page_fault = [&](addr_t addr) {
    std::stringstream pg_info;
    pg_info << "used_pages " << used_pages << " max " << max_active_pg_cnt;

    NVSL_ASSERT(used_pages <= max_active_pg_cnt, pg_info.str());

    const auto pg_idx = (addr - RCast<addr_t>(get_shm())) >> 12;
    const auto addr_p = RCast<void *>(addr);

    DBGH(2) << "Got fault for page idx " << pg_idx << "\n";
    DBGH(3) << pg_info.str() << std::endl;

    if (used_pages == max_active_pg_cnt) {
      DBGH(2) << "Page replacement: start evict " << addr_p << "\n";
      evict_a_page();
    } else {
      DBGH(2) << "Page replacement: did not run out of page " << addr_p << "\n";
    }

    faults++;

    used_pages++;
    void *buf = map_page_from_blkdev(addr);

    DBGH(2) << "Replacing page done " << RCast<void *>(addr) << "\n";
    return buf;
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

void Controller::flush_cache() {
  for (const auto [page_idx, _] : mapped_pages) {
    const auto page_addr = shm_start + (page_idx << 12);
    ubd->write_blocking(P(page_addr), page_idx * 8, 8);
  }

  mapped_pages.clear();
  init(max_active_pg_cnt, shm_pg_cnt);
}

Controller::Controller() {
  ubd = new UserBlkDev;
  pfm = new PFMonitor;
  mbd = new MemBWDist;
  nbd = new NumaBinder();

  page_size = (uint64_t)sysconf(_SC_PAGESIZE);

  mbd->start_sampling(10);
  ubd->init();
  pfm->init(REMOTE_NODE);
}

/** @brief Initialize the internal state **/
int Controller::init(std::size_t max_active_pg_cnt /* = 2 */,
                     std::size_t shm_pg_cnt /* = (64 * 1024UL) */) {
  int rc = 0;

  this->max_active_pg_cnt = max_active_pg_cnt;
  this->shm_pg_cnt = shm_pg_cnt;

  this->faults.reset();

  shm_size = shm_pg_cnt * page_size;
  used_pages = 0;

  nbd->bind_to_node(REMOTE_NODE);

  if (shm_start) {
    std::cerr << "Unmapping shm\n";
    rc = munmap(shm_start, shm_size);
    if (rc == -1) {
      DBGE << "Unable to munmap\n";
    }
  }

  const auto prot = PROT_READ | PROT_WRITE;
  const auto flags = MAP_ANONYMOUS | MAP_PRIVATE;
  auto shm_addr = mmap(nullptr, shm_size, prot, flags, -1, 0);

  if (shm_addr == MAP_FAILED) {
    DBGE << "mmap call for allocating shared memory failed" << std::endl;
    DBGE << PSTR() << std::endl;
    return -1;
  }

  nbd->bind_to_node(0);

  shm_start = RCast<char *>(shm_addr);

  rc = pfm->register_range(shm_addr, shm_size);

  if (rc == -1) {
    DBGE << "Failed to register the range for the page fault handler"
         << std::endl;
    return -1;
  }

  if (not monitor_thread_running) {
    pthread_t pf_thread;
    rc = pthread_create(&pf_thread, nullptr, &monitor_thread_wrapper, this);

    if (rc == -1) {
      DBGE << "Unable to monitor for page fault" << std::endl;
      return -1;
    }

    monitor_thread_running = true;
  }

  return 0;
}

void Controller::resize_cache(size_t pg_cnt) {
  const auto should_flush = pg_cnt < this->max_active_pg_cnt;
  this->max_active_pg_cnt = pg_cnt;

  if (should_flush) {
    std::cerr << "Cache size reduced, flushing...\n";
    this->flush_cache();
  } else {
    std::cerr << "Cache size not reduced.\n";
  }
}

void Controller::reset_stats() {
  this->faults.reset();
}

void *Controller::get_shm() {
  return shm_start;
}

uint64_t Controller::get_shm_len() {
  return shm_pg_cnt << 12;
}

void *Controller::get_shm_end() {
  return RCast<char *>(shm_start) + (shm_pg_cnt << 12);
}
