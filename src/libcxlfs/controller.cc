// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   pfcontroller.cc
 * @date   novembre  16, 2022
 * @brief  Controller to deal with page fault
 */


#include <functional>

#include "libcxlfs/controller.hh"
#include <pthread.h>

#include "nvsl/common.hh"
#include "nvsl/stats.hh"
#include "nvsl/utils.hh"
#include "nvsl/error.hh"

using nvsl::RCast;

PFMonitor::addr_t Controller::get_avaible_page() {
  const auto shared_mem_start_npt =
      RCast<uint64_t>(RCast<char *>(shared_mem_start));
  const auto page_addr = ((shared_mem_start_npt >> 12) + page_use) << 12;

  page_use += 1;

  if (page_use == page_size) {
    run_out_mapped_page = true;
  }

  return page_addr;
}

PFMonitor::addr_t Controller::evict_a_page(PFMonitor::addr_t start_addr,
                                           PFMonitor::addr_t end_addr) {
  // const auto start = RCast<uint64_t>(start);
  // const auto end = RCast<uint64_t>(end);

  const auto dist = mbd->get_dist(start_addr, end_addr);

  PFMonitor::addr_t target_page;

  for (uint64_t i = 0; i < page_count; i++) {
    if (dist[i]) {
      target_page = i;
      break;
    }
  }

  const auto addr = target_page << 12;

  return addr;
}

// PFMonitor::Callback Controller::notify_page_fault(PFMonitor::addr_t addr) {
//     if(run_out_mapped_page) {
//         const auto page_addr = evict_a_page(RCast<uint64_t>(RCast<char *>(shared_mem_start)), RCast<uint64_t>(RCast<char *>(shared_mem_start)_end));
//         map_new_page_from_blkdev(addr, page_addr);
//     } else {
//         const auto page_addr = get_avaible_page();
//         map_new_page_from_blkdev(addr, page_addr);
//     }

//     return nullptr;

// }

int Controller::map_new_page_from_blkdev(PFMonitor::addr_t pf_addr,
                                         PFMonitor::addr_t map_addr) {

  char *buf = RCast<char *>(malloc(page_size));

  const auto addr_off =
      RCast<uint64_t>(pf_addr) - RCast<uint64_t>(this->shared_mem_start);
  const auto start_lba = (addr_off >> 12) * 8;

  ubd->read_blocking(buf, start_lba, 8);

  const auto map_addr_pt = RCast<uint64_t *>(map_addr);
  DBGH(2) << map_addr_pt << " " << pf_addr << "\n";

  void *mmap_addr = mmap((void *)map_addr_pt, 0x1000, PROT_READ | PROT_WRITE,
                         MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

  if ((mmap_addr == MAP_FAILED) and (mmap_addr == (void *)map_addr_pt)) {
    DBGE << "mmap for handling page fault failed. Reason: " << PSTR()
         << std::endl;
    exit(1);
  }

  DBGH(2) << "Memcpying the page from the blkdev to memory\n";
  memcpy((void *)map_addr_pt, buf, page_size);

  return 1;
}

void Controller::monitor_thread() {
  PFMonitor::Callback notify_page_fault = [&](PFMonitor::addr_t addr) {
    DBGH(2) << "Replacing page " << RCast<void *>(addr) << "\n";

    if (run_out_mapped_page) {
      DBGH(2) << "Replacing page: start evict " << RCast<void *>(addr) << "\n";
      const auto page_addr =
          evict_a_page(RCast<uint64_t>(RCast<char *>(shared_mem_start)),
                       RCast<uint64_t>(shared_mem_start_end));
      map_new_page_from_blkdev(addr, page_addr);
    } else {
      DBGH(2) << "Replacing page: not ran out of page " << RCast<void *>(addr)
              << "\n";
      const auto page_addr = get_avaible_page();
      map_new_page_from_blkdev(addr, page_addr);
    }

    DBGH(2) << "Replacing page done " << RCast<void *>(addr) << "\n";
  };
  pfm->monitor(notify_page_fault);
  return;
}

void Controller::write_to_ubd(void *buf, char *addr) {

  const auto addr_off =
      RCast<uint64_t>(addr) - RCast<uint64_t>(this->shared_mem_start);
  const auto addr_page = (addr_off >> 12);
  const auto start_lba = addr_page * 8;

  DBGH(2) << "Write to " << RCast<void *>(addr) << "\n";

  ubd->write_blocking(buf, start_lba, 8);
  return;
}

void Controller::read_from_ubd(void *buf, char *addr) {
  const auto addr_off =
      RCast<uint64_t>(addr) - RCast<uint64_t>(this->shared_mem_start);
  const auto addr_page = (addr_off >> 12);
  const auto start_lba = addr_page * 8;

  DBGH(2) << "Read from " << RCast<uint64_t>(addr) << "\n";

  ubd->read_blocking(buf, start_lba, 8);
  return;
}

/** @brief Initialize the internal state **/
int Controller::init() {
  ubd = new UserBlkDev();
  pfm = new PFMonitor();
  mbd = new MemBWDist();

  ubd->init();
  pfm->init();

  run_out_mapped_page = false;

  page_size = 0x1000;
  page_count = 1000;
  page_use = 0;

  // shared_mem_start = RCast<char*>(malloc(page_size * page_count));
  void *temp = mmap(nullptr, page_size * page_count, PROT_READ | PROT_WRITE,
                    MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

  if (temp == MAP_FAILED) {
    DBGE << "map failed \n";
    DBGE << PSTR();
    exit(1);
  }
  // shared_mem_start = RCast<char *>(mmap(nullptr, page_size*page_count,
  // PROT_READ | PROT_WRITE,
  //      MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0));

  shared_mem_start = RCast<char *>(temp);

  // *shared_mem_start = 0xd;

  // RCast<char *>(shared_mem_start)_end = page_size * page_count + RCast<char
  // *>(shared_mem_start);

  const auto rc = pfm->register_range(RCast<uint64_t *>(shared_mem_start),
                                      page_size * page_count);
  if (rc != 0) {
    perror("pfm->register_range");
  }

  pthread_t thr;

  // auto wrapper = [&](void *arg){
  //     static_cast<void *>(arg)->monitor_thread();
  // };

  pthread_create(&thr, nullptr, &Controller::monitor_thread_wrapper, this);

  return 1;
}

char *Controller::getSharedMemAddr() {
  return shared_mem_start;
}

uint64_t Controller::getSharedMemSize() {
  return page_size;
}
