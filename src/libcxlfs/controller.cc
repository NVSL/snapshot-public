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

#include "libcxlfs/membwdist.hh"
#include "libcxlfs/pfmonitor.hh"
#include "nvsl/common.hh"
#include "nvsl/error.hh"
#include "nvsl/stats.hh"
#include "nvsl/utils.hh"
#include "nvsl/envvars.hh"

NVSL_DECL_ENV(REMOTE_NODE);

using nvsl::P;
using nvsl::RCast;
using addr_t = PFMonitor::addr_t;

size_t MAX_DIST_AGE = 100;

static unsigned int g_seed;
MemBWDist::dist_t dist;
size_t dist_age = MAX_DIST_AGE;

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

#ifdef CXLBUF_TESTING_GOODIES
  page_eviction_clk.tick();
#endif // CXLBUF_TESTING_GOODIES

  if (dist_age == MAX_DIST_AGE) {
    nvsl::Clock clk;
    clk.tick();
    dist = mbd->get_dist(rg_start, rg_end);
    clk.tock();

    if (clk.ns() / 1024 > 1) {
      MAX_DIST_AGE *= (clk.ns() / 1024);
    }

    dist_age = 0;
  } else {
    dist_age++;
  }

  addr_t target_page_idx = -1;

#ifdef CXLBUF_TESTING_GOODIES
  tgt_pg_calc_clk.tick();
#endif // CXLBUF_TESTING_GOODIES

  mbd_dist_sz += dist.size();

  size_t pg_lookup_cnt = 0;
  for (const auto [pg_idx, _] : mapped_pages) {
    if (!dist.contains(pg_idx)) {
      target_page_idx = pg_idx;
    }

    if (pg_lookup_cnt++ > MAX_MAPPED_PG_LOOKUP) {
      break;
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

#ifdef CXLBUF_TESTING_GOODIES
  tgt_pg_calc_clk.tock();
  blk_wb_clk.tick();
#endif // CXLBUF_TESTING_GOODIES

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

#ifdef CXLBUF_TESTING_GOODIES
  blk_wb_clk.tock();
  wb_nvme_wb_clk.tick();
#endif // CXLBUF_TESTING_GOODIES

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

#ifdef CXLBUF_TESTING_GOODIES
  wb_nvme_wb_clk.tock();
#endif // CXLBUF_TESTING_GOODIES

  int rc = mprotect(target_page, page_size, PROT_NONE);

  if (rc == -1) {
    DBGE << "mprotect failed: " << PSTR() << "\n";
    exit(1);
  }

  mapped_pages.erase(target_page_idx);
  used_pages--;

#ifdef CXLBUF_TESTING_GOODIES
  page_eviction_clk.tock();
#endif // CXLBUF_TESTING_GOODIES

  return 0;
}

void *Controller::map_page_from_blkdev(addr_t pf_addr) {
  void *buf = malloc(page_size);
  pf_addr = (pf_addr >> 12) << 12;

  const auto addr_off = RCast<uint64_t>(pf_addr) - RCast<uint64_t>(shm_start);
  const auto start_lba = (addr_off >> 12) * 8;

  DBGH(3) << "Getting lba " << start_lba << std::endl;

#ifdef CXLBUF_TESTING_GOODIES
  blk_rd_clk.tick();
#endif // CXLBUF_TESTING_GOODIES

  ubd->read_blocking(buf, start_lba, 8);

#ifdef CXLBUF_TESTING_GOODIES
  blk_rd_clk.tock();
#endif // CXLBUF_TESTING_GOODIES

  DBGH(3) << "Adding the page to the mapped page list" << std::endl;
  mapped_pages[((pf_addr - RCast<uint64_t>(get_shm())) >> 12)] = true;

  return buf;
}

void Controller::monitor_thread() {
  nbd->bind_to_node(remote_node);
  PFMonitor::Callback notify_page_fault = [&](addr_t addr) {
#ifdef CXLBUF_TESTING_GOODIES
    page_fault_clk.tick();
#endif // CXLBUF_TESTING_GOODIES
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

#ifdef CXLBUF_TESTING_GOODIES
    page_fault_clk.tock();
#endif // CXLBUF_TESTING_GOODIES
    return buf;
  };

  std::cerr << "calling pfm::monitor\n";
  if (-1 == pfm->monitor(notify_page_fault)) {
    DBGE << "Unable to start page fault handler\n";
    exit(1);
  }
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
  if (shm_pg_cnt == 0) {
    NVSL_ERROR(
        "Flush cache called without initializing the controller first\n");
  }

  std::cerr << "Flushing caches\n";
  for (const auto [page_idx, _] : mapped_pages) {
    const auto page_addr = shm_start + (page_idx << 12);
    ubd->write_blocking(P(page_addr), page_idx * 8, 8);
  }

  mapped_pages.clear();
  init(max_active_pg_cnt, shm_pg_cnt);
}

Controller::Controller() {
  this->remote_node = std::stoi(get_env_str(REMOTE_NODE_ENV, "0"));

  std::cerr << "remote node is " << remote_node << "\n";

  ubd = new UserBlkDev;
  pfm = new PFMonitor;
  mbd = new MemBWDist;
  nbd = new NumaBinder();

  mbd_dist_sz.reset();

  page_size = (uint64_t)sysconf(_SC_PAGESIZE);

  mbd->start_sampling(250);
  ubd->init();
  pfm->init(remote_node);
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

  nbd->bind_to_node(this->remote_node);

  if (shm_start) {
    DBGH(1) << "Unmapping shm\n";
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
    DBGE << "mmap call: "
         << nvsl::mmap_to_str(nullptr, shm_size, prot, flags, -1, 0) << "\n";
    DBGE << PSTR() << std::endl;
    return -1;
  }

  nbd->bind_to_node(0);

  shm_start = RCast<char *>(shm_addr);

  rc = mprotect(shm_start, shm_size, PROT_NONE);
  pfm->register_range(shm_start, shm_size);

  if (rc == -1) {
    DBGE << "Failed to register the range using mprotect: " << PSTR()
         << std::endl;
    return -1;
  }

  if (not monitor_thread_running) {
    std::cerr << "starting monitor thread\n";
    pthread_t pf_thread;
    rc = pthread_create(&pf_thread, nullptr, &monitor_thread_wrapper, this);

    if (rc == -1) {
      DBGE << "Unable to monitor for page fault" << std::endl;
      return -1;
    }

    monitor_thread_running = true;
  }

  sleep(1);

  return 0;
}

void Controller::resize_cache(size_t pg_cnt) {
  const auto should_flush = pg_cnt < this->max_active_pg_cnt;
  this->max_active_pg_cnt = pg_cnt;

  if (should_flush) {
    DBGH(0) << "Cache size reduced, flushing...\n";
    this->flush_cache();
  } else {
    DBGH(0) << "Cache size not reduced.\n";
  }
}

void Controller::reset_stats() {
  this->faults.reset();
  mbd->get_dist_lat.reset();
  this->page_eviction_clk.reset();
  this->blk_rd_clk.reset();
  this->page_fault_clk.reset();
  this->tgt_pg_calc_clk.reset();
  this->blk_wb_clk.reset();
  this->wb_nvme_wb_clk.reset();
  this->mbd_dist_sz.reset();

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

std::unordered_map<std::string, nvsl::Clock> Controller::get_clocks() {
  return {
      {"mbd.get_dist_lat", mbd->get_dist_lat},
      {"blk_rd_clk", blk_rd_clk},
      {"page_eviction_clk", page_eviction_clk},
      {"page_fault_clk", page_fault_clk},
      {"tgt_pg_calc_clk", tgt_pg_calc_clk},
      {"blk_wb_clk", blk_wb_clk},
      {"wb_nvme_wb_clk", wb_nvme_wb_clk},
  };
}

std::unordered_map<std::string, size_t> Controller::get_stats() {
  return {
    {"mbd_dist_sz.avg", mbd_dist_sz.avg()},
    {"mbd_dist_sz.max", mbd_dist_sz.max()},
  };
}
