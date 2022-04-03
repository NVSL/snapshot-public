// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   log.cc
 * @date   mars 25, 2022
 * @brief  Brief description here
 */

#include <cassert>
#include <filesystem>
#include <thread>

#include "log.hh"
#include "nvsl/stats.hh"
#include "nvsl/utils.hh"
#include "nvsl/pmemops.hh"
// #include "libdsaemu.hh"

nvsl::Counter *nvsl::cxlbuf::skip_check_count,
  *nvsl::cxlbuf::logged_check_count;
nvsl::StatsFreq<> *nvsl::cxlbuf::tx_log_count_dist;

extern nvsl::PMemOps *pmemops;
using namespace nvsl;

void cxlbuf::Log::log_range(void *start, size_t bytes) {
  auto &log_entry = *(log_entry_t*)((char*)log_area->entries
                                    + log_area->log_offset);
    
  if (log_area != nullptr and cxlModeEnabled) {
    storeInstEnabled = true;

    NVSL_ASSERT((bytes % 8 == 0) and (bytes < 4096), "");

    /* Update the volatile address list */
    this->entries.emplace_back((size_t)start, bytes);
    
    /* Write to the persistent log and flush and fence it */
    log_entry.addr = (uint64_t)start;
    log_entry.bytes = bytes/8;

    real_memcpy(&log_entry.val, start, bytes);

    const size_t entry_sz = sizeof(log_entry_t) + bytes;
    log_area->log_offset += entry_sz;

    const size_t cls_to_flush 
      = (log_area->log_offset - last_flush_offset + 63) / 64;

    DBGH(4) << "Entry size = " << entry_sz << " bytes."
            << " last_flush_offset = " << last_flush_offset
            << " log_area->log_offset = " << log_area->log_offset << std::endl;

    /* Flush all the lines only if the current entry ends at a cacheline
       boundary */
    if (log_area->log_offset%64 == 0){
      DBGH(4) << "[1] Flushing " << cls_to_flush << " cachelines starting at "
              << (void*)((char*)log_area + last_flush_offset) << std::endl;

      pmemops->flush((char*)log_area + last_flush_offset, cls_to_flush * 64);

      last_flush_offset = log_area->log_offset;
    } else {
      if (cls_to_flush > 1) {
        DBGH(4) << "[2] Flushing " << cls_to_flush - 1
                << " cachelines starting at "
                << (void*)((char*)log_area + last_flush_offset) << std::endl;
        
        /* FLush all but the last cacheline. Last (partially written) cacheline
         * will be flushed with the next entry or on snapshot */
        pmemops->flush((char*)log_area + last_flush_offset,
                       (cls_to_flush - 1) * 64);
        last_flush_offset += (cls_to_flush - 1) * 64;
      }
    }

#ifndef RELEASE
    ++*logged_check_count;
#endif

    /*
     * Align the log offset to the next cacheline.
     * This reduces the number of clwb needed
     */

    // log_area->log_offset = ((log_area->log_offset+63)/64)*64;

    NVSL_ASSERT(log_area->log_offset < BUF_SIZE, "");
  }
}


void cxlbuf::Log::flush_all() const {
  if (this->last_flush_offset != this->log_area->log_offset) {
    const void* start = (char*)log_area->entries + last_flush_offset;
    const size_t len = this->log_area->log_offset - this->last_flush_offset;

    DBGH(3) << "Flushing unflushed " << len << " bytes" << std::endl; 
    
    pmemops->flush((void*)start, len);
  }
}

std::tuple<cxlbuf::Log::log_layout_t*, fs::path>
cxlbuf::Log::get_log_by_id(const std::string &name,
                            void *addr /* = nullptr */) {
  const auto log_fname = fs::path(Log::LOG_LOC) / fs::path(name + ".log");

  /* Check and open the log file */
  if (not fs::is_regular_file(log_fname)) {
    DBGE << "Requested log file " << log_fname << " does not exist."
         << std::endl;
  }

  int fd = open(log_fname.c_str(), O_RDWR);
  if (fd == -1) {
    DBGE << "Unable to open log file " << log_fname << ": " << std::endl;
    DBGE << PSTR() << std::endl;;
    exit(1);
  }

  /* mmap the log file */
  const auto fsize = fs::file_size(log_fname);
  const auto flags = MAP_SHARED;
  const auto prot = PROT_READ | PROT_WRITE;

  DBGH(4) << "Calling real_" << mmap_to_str(addr, fsize, prot, flags, fd, 0)
          << " for log " << fs::path(log_fname) << std::endl;
  auto log_ptr = (log_layout_t*)real_mmap(addr, fsize, prot, flags, fd, 0);

  if (log_ptr == (log_layout_t*)-1) {
    DBGE << "Unable to mmap log file " << log_fname << ": " << std::endl;
    DBGE << PSTR() << std::endl;;
    exit(1);
  }  
  
  return {log_ptr, log_fname};
}

void cxlbuf::Log::init_dirs() {
  DBGH(1) << "Creating log directory " << fs::path(LOG_LOC) << std::endl;
  if (not std::filesystem::is_directory(S(LOG_LOC))) {
    std::filesystem::create_directory(S(LOG_LOC));
  }
}

void cxlbuf::Log::init_thread_buf() {
  const auto tid = pthread_self();
  const auto pid = getpid();

  const auto fname = S(pid) + "." + S(tid) + ".log";
  const auto fpath = S(LOG_LOC) + "/" + fname;

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


  log_area = (log_layout_t*)real_mmap(nullptr, BUF_SIZE, PROT_READ | PROT_WRITE,
                                      MAP_SYNC | MAP_SHARED_VALIDATE, fd, 0);

  if (log_area == (log_layout_t*)-1) {
    perror("mmap for buffer failed");
    exit(1);
  }
  
  close(fd);

}

cxlbuf::Log::Log() {
  entries.reserve(Log::MAX_ENTRIES);

  this->init_dirs();
  this->init_thread_buf();
}

thread_local cxlbuf::Log tls_log;
