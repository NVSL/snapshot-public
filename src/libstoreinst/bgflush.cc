// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   bgflush.cc
 * @date   avril  5, 2022
 * @brief  Brief description here
 */

#include "bgflush.hh"
#include <pthread.h>
#include "libstoreinst.hh"
#include "nvsl/pmemops.hh"
#include <iostream>

using namespace nvsl::cxlbuf;

static size_t queue_head, queue_tail;

static bgflush::bgf_job_t job_queue[bgflush::BGF_JOB_QUEUE_MAX_LEN];

void bgflush::push(const bgflush::bgf_job_t entry) {
  // DBGH(3) << "{} queue address " << &job_queue << std::endl;
  // DBGH(3) << "Pushing entry to flush queue: {" << entry.addr << ", "
  //         << entry.bytes << "}" << std::endl;
  job_queue[queue_tail].addr = entry.addr;
  job_queue[queue_tail].bytes = entry.bytes;

  queue_tail++;
}

void* bgflush_thread(void*) {
  while (true) {
    if (queue_head != queue_tail) {
      const auto &entry = job_queue[queue_tail];
      DBGH(3) << "queue address " << &job_queue << std::endl;
      DBGH(3) << "BG FLUSH processing " << entry.addr << " +" << entry.bytes
              << std::endl;
      pmemops->flush(entry.addr, entry.bytes);
      queue_head++;
    }
  }
}

void bgflush::launch() {
  clear();
  pthread_t tid;
  pthread_create(&tid, NULL, &bgflush_thread, NULL);
  pthread_detach(tid);
}

void bgflush::drain() {
  while (queue_head != queue_tail);
}

void bgflush::clear() {
  queue_head = queue_tail = 0;
}


