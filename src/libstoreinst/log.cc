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
#include "nvsl/pmemops.hh"
// #include "libdsaemu.hh"

nvsl::Counter *nvsl::cxlbuf::skip_check_count,
  *nvsl::cxlbuf::logged_check_count;
nvsl::StatsFreq<> *nvsl::cxlbuf::tx_log_count_dist;

extern nvsl::PMemOps *pmemops;
using namespace nvsl;

void cxlbuf::Log::log_range(void *start, size_t bytes) {
  auto &log_entry = *(log_entry_t*)((char*)log_area->entries + current_log_off);
    
  if (log_area != nullptr and cxlModeEnabled) {
    storeInstEnabled = true;

    /* Since we recycle log, reset the state if it was committed before this
       operation */
    if (this->get_state() == State::COMMITTED) {
      this->set_state(State::EMPTY);
    }

    /* Update the volatile address list */
    this->entries.emplace_back((size_t)start, bytes);
    
    /* Write to the persistent log and flush and fence it */
    log_entry.addr = (uint64_t)start;
    log_entry.bytes = bytes;
    real_memcpy(&log_entry.val, start, bytes);
    pmemops->flush(&log_entry, sizeof(log_entry) + bytes);

    ++*logged_check_count;

    /* Align the log offset to the next cacheline. This reduces the number of
       clwb needed */
    current_log_off += sizeof(log_entry_t) + bytes;
    current_log_off = ((current_log_off+63)/64)*64;

    assert(current_log_off < BUF_SIZE);
  }
}

cxlbuf::Log::log_layout_t
*cxlbuf::Log::get_log_by_id(const std::string &name) {
  const auto log_fname = fs::path(Log::LOG_LOC) / fs::path(name + ".log");

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

  const auto fsize = fs::file_size(log_fname);
  const auto flags = MAP_PRIVATE;
  const auto prot = PROT_READ | PROT_WRITE;
  auto log_ptr = (log_layout_t*)real_mmap(nullptr, fsize, prot, flags, fd, 0);

  if (log_ptr == (log_layout_t*)-1) {
    DBGE << "Unable to mmap log file " << log_fname << ": " << std::endl;
    DBGE << PSTR() << std::endl;;
    exit(1);
  }  
  
  return log_ptr;
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
