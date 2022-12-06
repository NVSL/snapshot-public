// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   membwdist.cc
 * @date   novembre 14, 2022
 * @brief  Brief description here
 */

#include <asm-generic/errno-base.h>
#include <cmath>
#include <cstdint>

#include <asm/unistd.h>
#include <linux/hw_breakpoint.h>
#include <linux/perf_event.h>
#include <sys/mman.h>
#include <unistd.h>

#include "libcxlfs/membwdist.hh"
#include "nvsl/common.hh"
#include "nvsl/stats.hh"
#include "nvsl/utils.hh"

using nvsl::P;
using nvsl::RCast;

long perf_event_open(struct perf_event_attr *hw_event, pid_t pid, int cpu,
                     int group_fd, unsigned long flags) {
  int ret;

  ret = syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
  return ret;
}

__attribute__((__noinline__)) void *get_pc() {
  return __builtin_return_address(0);
}

int MemBWDist::start_sampling(size_t sampling_freq) {
  int pid = 0;
  int cpu = -1;
  int fd = -1;
  int rc = 0;
  perf_event_attr attr = {0};

  attr.type = PERF_TYPE_RAW;
  attr.size = sizeof(perf_event_attr);
  attr.config = 0x20D1; /* MEM_LOAD_RETIRED.L3_MISS */
  attr.disabled = 0;
  attr.exclude_kernel = 1;
  attr.exclude_hv = 1; /* Skip hypervisor */
  attr.precise_ip = 2; /* 0 skid requested */
  attr.sample_period = sampling_freq;
  attr.sample_type = PERF_SAMPLE_IP | PERF_SAMPLE_ADDR;

  DBGH(3) << "Calling perf_event_open(...)\n";

  fd = perf_event_open(&attr, pid, cpu, -1, 0);
  DBGH(0) << "fd " << fd << " attr.read_format " << attr.read_format
          << " attr.sample_type " << attr.sample_type << std::endl;

  if (fd == -1) {
    DBGE << "Error opening perf leader " << (char *)attr.config << std::endl;
    return fd;
  }

  const size_t mmap_size = (BUF_SZ_PG_CNT + 1) * sysconf(_SC_PAGESIZE);
  auto buf_raw =
      mmap(nullptr, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (buf_raw == MAP_FAILED) {
    DBGE << nvsl::mmap_to_str(nullptr, mmap_size, PROT_READ | PROT_WRITE,
                              MAP_SHARED, fd, 0)
         << " for perf failed: " << PSTR() << "\n";
    return -1;
  }

  this->sample_buf = RCast<perf_event_mmap_page *>(buf_raw);

  close(fd);

  return rc;
}

std::vector<MemBWDist::record_t> MemBWDist::get_samples() {
  std::vector<record_t> result;
  char *samples;
  size_t sample_cnt = 0;

  DBGH(1) << "sample_buf->data_head = " << sample_buf->data_head
          << " sample_buf->data_tail = " << sample_buf->data_tail << std::endl;

  auto &tail = sample_buf->data_tail;
  auto &head = sample_buf->data_head;

  samples = RCast<char *>(this->sample_buf) + this->sample_buf->data_offset;

  while (tail != head) {
    const auto sample_addr = samples + (tail % this->sample_buf->data_size);
    const auto *header = RCast<perf_event_header *>(sample_addr);

    if (header->type == PERF_RECORD_SAMPLE) {
      const auto sample = RCast<const pebs_sample_t *>(header);

      result.push_back({sample->ip, sample->addr});

      sample_cnt++;
      tail += header->size;
    } else {
    }
  }

  DBGH(2) << "Found " << sample_cnt << " samples\n";

  return result;
}

MemBWDist::dist_t MemBWDist::get_dist(addr_t start, addr_t end) {
  get_dist_lat.tick();

  char *samples;
  size_t sample_cnt = 0, useful_samples = 0;
  float useful_samples_pct = 0;

  const size_t pg_sz_bits = (uint)log2(sysconf(_SC_PAGESIZE));

  const auto start_pg = start >> pg_sz_bits;
  const auto end_pg = end >> pg_sz_bits;

  dist_t result;

  auto &tail = sample_buf->data_tail;
  auto &head = sample_buf->data_head;

  DBGH(3) << "[" << start << "," << end << "] -> "
          << "[" << start_pg << "," << end_pg << "]\n";

  samples = RCast<char *>(sample_buf) + sample_buf->data_offset;

  DBGH(1) << "head = " << head << " tail = " << tail << std::endl;

  while (tail != head) {
    const auto sample_addr = samples + (tail % sample_buf->data_size);
    const auto *header = RCast<const perf_event_header *>(sample_addr);

    if (header->type == PERF_RECORD_SAMPLE) {
      const auto sample = RCast<const pebs_sample_t *>(header);
      const auto addr_pg = sample->addr >> pg_sz_bits;

      if (addr_pg >= start_pg and addr_pg < end_pg) {
        result[addr_pg - start_pg] = true;
        useful_samples++;
      }

      sample_cnt++;

      if (sample_cnt > MAX_SAMPLES) {
        tail = head;
        break;
      }
      
      tail += header->size;
    } else {
      //      DBGE << "Unknown packet type" << std::endl;
    }
  }

  useful_samples_pct = (useful_samples * 100.0 / (float)sample_cnt);
  DBGH(2) << "Found " << sample_cnt << ", useful: " << useful_samples << " ("
          << useful_samples_pct << "%) samples\n";

  get_dist_lat.tock();

  return result;
}
