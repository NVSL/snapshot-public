// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   recovery.cc
 * @date   mars 29, 2022
 * @brief  Handles all the recovery stuff
 */

#include "libc_wrappers.hh"
#include "nvsl/string.hh"
#include "nvsl/utils.hh"
#include "recovery.hh"
#include "utils.hh"

#include <filesystem>
#include <sys/mman.h>

using namespace nvsl;

bool cxlbuf::PmemFile::needs_recovery() { return true; }

void cxlbuf::PmemFile::recover() {}

std::string cxlbuf::PmemFile::get_backing_fname() const {
    return this->path.string() + ".cxlbuf_backing";
}

bool cxlbuf::PmemFile::has_backing_file() {
  const auto bfile = fs::path(this->get_backing_fname());
  DBGH(3) << "Checking if backing file " << bfile << " exists" << std::endl;
  return std::filesystem::is_regular_file(bfile);
}

void cxlbuf::PmemFile::create_backing_file_internal() {
  const auto bfname = this->get_backing_fname();
  DBGH(2) << "Creating backing file...";
  fs::copy_file(this->path, bfname);
  DBG << "done" << std::endl;
}

void cxlbuf::PmemFile::create_backing_file() {
  if (not this->has_backing_file()) {
    /* Only copy to the backing file if this is the first time */
    this->create_backing_file_internal();
  }
}

void cxlbuf::PmemFile::map_backing_file() {
  const auto bfname = this->get_backing_fname();

  /* Since this file is opened in the PM, create a corresponding backing file
     and map it at +0x10000000000 */
  void *bck_addr = (char*)this->addr + 0x10000000000;
  const int bck_fd = open(bfname.c_str(), O_RDWR);

  if (bck_fd == -1) {
    perror("Unable to open backing file");
    exit(1);
  }
  
  const int bck_flags = MAP_SHARED_VALIDATE | MAP_SYNC | MAP_FIXED_NOREPLACE;
  const void *mbck_addr = real_mmap(bck_addr, this->len, bck_flags,
                                    PROT_READ | PROT_WRITE, bck_fd, 0);

  if (mbck_addr == (void*)-1) {
    DBGE << "Unable to map backing file" << std::endl;
    exit(1);
  } else {
    DBGH(2) << "Backing file " << bfname << " for " << fs::path(this->path)
            << " mapped at address " << mbck_addr << std::endl;
  }  
}

cxlbuf::PmemFile::PmemFile(const fs::path &path, void* addr, size_t len)
  : path(path), addr(addr), len(len) {

  if (this->needs_recovery()) {
    recover();
  }
}

void cxlbuf::PmemFile::set_addr(void *addr) {
  this->addr = addr;
}

void *cxlbuf::PmemFile::map_to_page_cache(int flags, int prot, int fd, off_t off) {
  /* Add map fixed no replace to prevent kernel from mapping it outside the
     tracking space and remove the MAP_SYNC flag */
  if (this->addr != nullptr) {
    flags |= MAP_FIXED_NOREPLACE;
  }
  flags &= ~MAP_SYNC;

  DBGH(2) << "Calling real_mmap: "
          << mmap_to_str(this->addr, this->len, prot, flags, fd, off)
          << std::endl;
  void *result = real_mmap(this->addr, this->len, prot, flags, fd, off);

  if (result != nullptr) {
    const cxlbuf::addr_range_t val
      = {(size_t)(this->addr), (size_t)((char*)this->addr + this->len)};
    cxlbuf::mapped_addr.insert_or_assign(fd, val);
    DBGH(2) << "mmap_addr recorded " << fd << " -> " << (void*)val.start << ", "
            << (void*)val.end << std::endl;

    if (not addr_in_range(result)) {
      DBGE << "Cannot map pmem file in range" << std::endl;
      DBGE << "Asked " << this->addr << ", got " << result << std::endl;
      perror("mmap:");
      system(("cat /proc/" + std::to_string(getpid()) + "/maps >&2").c_str());
      exit(1);
    }
  } else {
    DBGW << "Call to mmap failed" << std::endl;
  }

  return result;
}
