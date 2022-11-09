// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   log.cc
 * @date   mars 25, 2022
 * @brief  Brief description here
 */

#include <cassert>
#include <dlfcn.h>
#include <filesystem>
#include <thread>

#include "bgflush.hh"
#include "libvram/libvram.hh"
#include "log.hh"
#include "nvsl/clock.hh"
#include "nvsl/pmemops.hh"
#include "nvsl/stats.hh"
#include "nvsl/utils.hh"

#if defined LOG_FORMAT_VOLATILE && defined LOG_FORMAT_NON_VOLATILE
#error \
    "Incompatible flags: LOG_FORMAT_VOLATILE and LOG_FORMAT_NON_VOLATILE enabled"
#endif

nvsl::Counter *nvsl::cxlbuf::skip_check_count,
    *nvsl::cxlbuf::logged_check_count;
nvsl::StatsFreq<> *nvsl::cxlbuf::tx_log_count_dist;

extern nvsl::PMemOps *pmemops;
using namespace nvsl;

void cxlbuf::Log::log_range(void *start, size_t bytes) {
  auto cxlModeEnabled_reg = cxlModeEnabled;
  auto &log_entry = *RCast<log_entry_t *>(log_area->tail_ptr);

#ifdef NO_PERSIST_OPS
  return;
#endif

#ifndef NDEBUG
  NVSL_ASSERT(log_area != nullptr, "Logging called without a log area");
#endif

  if (cxlModeEnabled_reg) [[likely]] {
    storeInstEnabled = true;

#ifdef CXLBUF_TESTING_GOODIES
    perst_overhead_clk->tick();
#endif // CXLBUF_TESTING_GOODIES

    // #ifndef NDEBUG
    NVSL_ASSERT((bytes < (1 << 22)), "Log request to location " +
                                         S((void *)start) + " for " + S(bytes) +
                                         " bytes is invalid");
    // #endif

#ifdef LOG_FORMAT_VOLATILE
    /* Update the volatile address list */
    this->entries.emplace_back((size_t)start, bytes);
#endif

    /* Write to the persistent log and flush and fence it */
    log_entry.addr = (uint64_t)start;
    log_entry.bytes = bytes;

    real_memcpy(&log_entry.content, start, bytes);

    const size_t entry_sz = sizeof(log_entry_t) + bytes;
    log_area->log_offset += entry_sz;
    log_area->tail_ptr += entry_sz;

    const size_t cls_to_flush =
        (log_area->log_offset - last_flush_offset + 63) / 64;

    DBGH(4) << "Entry size = " << entry_sz << " bytes."
            << " address = " << (void *)start
            << " last_flush_offset = " << last_flush_offset
            << " log_area->log_offset = " << log_area->log_offset << std::endl;

    if (bytes == 8) {
      DBGH(4) << "Old value = " << (void *)(*(uint64_t *)start) << std::endl;
    }

    /* Flush all the lines only if the current entry ends at a cacheline
       boundary */
    if (log_area->log_offset % 64 == 0) {
      DBGH(4) << "[1] Flushing " << cls_to_flush << " cachelines starting at "
              << (void *)((char *)log_area + last_flush_offset) << std::endl;

      pmemops->flush((char *)log_area + last_flush_offset, cls_to_flush * 64);

      last_flush_offset = log_area->log_offset;
    } else {
      if (cls_to_flush > 1) {
        DBGH(4) << "[2] Flushing " << cls_to_flush - 1
                << " cachelines starting at "
                << (void *)((char *)log_area + last_flush_offset) << std::endl;

        /* FLush all but the last cacheline. Last (partially written) cacheline
         * will be flushed with the next entry or on snapshot */
        pmemops->flush(RCast<uint8_t *>(log_area) + last_flush_offset,
                       (cls_to_flush - 1) * 64);
        last_flush_offset += (cls_to_flush - 1) * 64;
      }
    }

#ifndef RELEASE
    ++*logged_check_count;
#endif

#ifdef TRACE_LOG_MSYNC
    if (startTracking) {
      std::cout << nvsl::get_stack_trace() << "\n";
    }
#endif

    NVSL_ASSERT(log_area->log_offset < BUF_SIZE, "");

#ifdef CXLBUF_TESTING_GOODIES
    perst_overhead_clk->tock();
#endif // CXLBUF_TESTING_GOODIES
  }
}

void cxlbuf::Log::flush_all() const {
  if (this->last_flush_offset != this->log_area->log_offset) {
    const void *start = (char *)log_area->content + last_flush_offset;
    const size_t len = this->log_area->log_offset - this->last_flush_offset;

    DBGH(3) << "Flushing unflushed " << len << " bytes" << std::endl;

    pmemops->flush((void *)start, len);
  }
}

std::tuple<cxlbuf::Log::log_layout_t *, fs::path>
cxlbuf::Log::get_log_by_id(const std::string &name,
                           void *addr /* = nullptr */) {
  const auto log_fname = fs::path(*log_loc) / fs::path(name + ".log");

  /* Check and open the log file */
  if (not fs::is_regular_file(log_fname)) {
    DBGE << "Requested log file " << log_fname << " does not exist."
         << std::endl;
  }

  int fd = open(log_fname.c_str(), O_RDWR);
  if (fd == -1) {
    DBGE << "Unable to open log file " << log_fname << ": " << std::endl;
    DBGE << PSTR() << std::endl;
    exit(1);
  }

  /* mmap the log file */
  const auto fsize = fs::file_size(log_fname);
  const auto flags = MAP_SHARED;
  const auto prot = PROT_READ | PROT_WRITE;

  DBGH(4) << "Calling real_" << mmap_to_str(addr, fsize, prot, flags, fd, 0)
          << " for log " << fs::path(log_fname) << std::endl;
  auto log_ptr = (log_layout_t *)real_mmap(addr, fsize, prot, flags, fd, 0);

  if (log_ptr == (log_layout_t *)-1) {
    DBGE << "Unable to mmap log file " << log_fname << ": " << std::endl;
    DBGE << PSTR() << std::endl;
    exit(1);
  }

  return {log_ptr, log_fname};
}

void nvsl::cxlbuf::Log::init_dirs() {
  DBGH(1) << "Creating log directory " << fs::path(*log_loc) << std::endl;
  if (not std::filesystem::is_directory(*log_loc)) {
    std::filesystem::create_directory(*log_loc);
  }
}

void nvsl::cxlbuf::Log::init_thread_buf() {
  const auto tid = pthread_self();
  const auto pid = getpid();

  const auto fname = S(pid) + "." + S(tid) + ".log";
  const auto fpath = *log_loc + "/" + fname;

  if (std::filesystem::is_regular_file(fpath)) {
    std::filesystem::remove_all(fpath);
  }

  DBGH(1) << "Creating buffer file " << fpath << std::endl;

  const int fd = open(fpath.c_str(), O_CREAT | O_RDWR, 0666);

  if (fd == -1) {
    perror("fallocate for buffer failed");
    exit(1);
  }

  if (-1 == fallocate(fd, 0, 0, BUF_SZ)) {
    perror("fallocate for buffer failed");
    exit(1);
  }

  if (is_prefix("/mnt/cxl0/", *log_loc)) {
    log_area = RCast<log_layout_t *>(nvsl::libvram::malloc(BUF_SIZE));
  } else {
    log_area = RCast<log_layout_t *>(
        real_mmap(nullptr, BUF_SIZE, PROT_READ | PROT_WRITE,
                  MAP_SYNC | MAP_SHARED_VALIDATE, fd, 0));
  }

  if (log_area == (log_layout_t *)-1) {
    perror("mmap for buffer failed");
    exit(1);
  }

  log_area->log_offset = 0;
  log_area->tail_ptr = RCast<uint8_t *>(log_area->content);

  close(fd);
}

nvsl::cxlbuf::Log::Log() {
#ifdef LOG_FORMAT_VOLATILE
  entries.reserve(Log::MAX_ENTRIES);
#endif

  this->init_dirs();
  this->init_thread_buf();
}

void nvsl::cxlbuf::cxlbuf_reg_tls_log() {
  if (tls_logs) {
    tls_logs = new std::vector<nvsl::cxlbuf::Log *>;
  }

  tls_logs->push_back(&tls_log);
}

thread_local nvsl::cxlbuf::Log thread_local_log;
std::vector<nvsl::cxlbuf::Log *> *tls_logs = nullptr;
