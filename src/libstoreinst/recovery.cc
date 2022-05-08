// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   recovery.cc
 * @date   mars 29, 2022
 * @brief  Handles all the recovery stuff
 */

#include "recovery.hh"
#include "libc_wrappers.hh"
#include "libstoreinst.hh"
#include "libvram/libvram.hh"
#include "log.hh"
#include "nvsl/string.hh"
#include "nvsl/utils.hh"
#include "utils.hh"

#include <fcntl.h>
#include <filesystem>
#include <sys/mman.h>
#include <sys/sendfile.h>
#include <unordered_set>

using namespace nvsl;

std::vector<std::string> cxlbuf::PmemFile::needs_recovery() const {
  std::vector<std::string> result = {};
  const auto dfname = this->get_dependency_fname();

  if (fs::is_regular_file(dfname)) {
    const std::ifstream df(dfname);
    std::stringstream contents;
    contents << df.rdbuf();

    const auto logs = split(contents.str(), "\n");

    DBGH(1) << "Found " << logs.size() << " logs in " << dfname << std::endl;

    for (const auto &log : logs) {
      DBGH(3) << "Checking log " << log << std::endl;

      const auto toks = split(log, ",", 3);
      const auto lfname = *log_loc + "/" + toks[0] + "." + toks[1] + ".log";

      if (fs::is_regular_file(lfname)) {
        DBGH(2) << "Found log " << log << std::endl;

        const auto [log_ptr, _] = Log::get_log_by_id(toks[0] + "." + toks[1]);

        if (log_ptr->state == Log::State::ACTIVE) {
          DBGH(2) << "Log needs recovery" << std::endl;

          result.push_back(log);
          break;
        } else {
          DBGH(2) << "Log does not need recovery. Log state = "
                  << (int)log_ptr->state << std::endl;
        }

        int mu_ret = real_munmap(log_ptr, fs::file_size(lfname));

        if (mu_ret == -1) {
          DBGE << "munmap for log failed" << std::endl;
          DBGE << PSTR();
          exit(1);
        }
      } else {
        DBGH(2) << "Log does not exist" << std::endl;
      }
    }
  }

  return result;
}

std::pair<void*, size_t> cxlbuf::PmemFile::get_map_dimensions() const {
  return {this->addr, this->len};
}

void cxlbuf::PmemFile::recover(const std::vector<std::string> &logs) {
  for (const auto &log : logs) {
    const auto toks = split(log, ",", 3);
    const auto pid = std::stoi(toks[0]);
    const auto tid = std::stoull(toks[1]);
    auto addr = (void*)std::stoull(toks[2]);

    DBGH(4) << "Log pid = " << pid << " tid = " << tid << " addr = "
            << (void*)addr << std::endl;

    const auto [log_ptr, lfname]
      = Log::get_log_by_id(S(pid) + "." + S(tid));

    DBGH(4) << "Total log size " << log_ptr->log_offset << " bytes" << std::endl;

    DBGH(1) << "Replaying log" << std::endl;

    // Prepare mmap arguments
    const auto prot = PROT_READ | PROT_WRITE;
    const auto flags = MAP_SHARED_VALIDATE | MAP_SYNC;
    const auto fpath = this->path.string() + ".cxlbuf_backing";
    const int fd = open(fpath.c_str(), O_RDWR);

    if (fd == -1) {
      DBGE << "Unable to open the file " << fpath << " to recover" << std::endl;
      DBGE << PSTR();
      exit(1);
    }

    // Map this file to the right address
    void *maddr = real_mmap(addr, this->len, prot, flags, fd, 0);

    if (maddr == (void*)-1) {
      DBGE << "Unable to mmap file " << fpath << " to recover" << std::endl;
      DBGE << PSTR();
      exit(1);
    }

    // Find log entries that apply to this file
    using log_entry_t = Log::log_entry_t;
    size_t cur_off = 0;
    while (cur_off < log_ptr->log_offset) {
      log_entry_t *entry = (log_entry_t*)&(((char*)log_ptr->content)[cur_off]);

      DBGH(4) << "Checking entry (" << entry->addr << ", " << entry->bytes
              << ")" << std::endl;
      
      if (((size_t)addr <= entry->addr) and
          (entry->addr < ((size_t)addr + this->len)) /*and entry->is_disabled != 1*/) {

        const auto dst_addr = (void*)(size_t)entry->addr;

        DBGH(4) << "Recovering location " << dst_addr << "...";

        // Write, flush and drain
        real_memcpy(dst_addr, entry->content, entry->bytes);
        pmemops->flush(dst_addr, entry->bytes);
        pmemops->drain();

        DBG << "done" << std::endl;
      } else {
        DBGH(4) << "No recovery needed"<< std::endl;
      }

      cur_off += entry->bytes + sizeof(log_entry_t);

      DBGH(4) << "New offset = " << cur_off << std::endl;
    }

    // Release all resources
    close(fd);

    int mu_ret = real_munmap(log_ptr, fs::file_size(lfname));

    if (mu_ret == -1) {
      DBGE << "munmap for log failed" << std::endl;
      DBGE << PSTR();
      exit(1);
    }

    mu_ret = real_munmap(addr, this->len);

    if (mu_ret == -1) {
      DBGE << "munmap for backing failed" << std::endl;
      DBGE << PSTR();
      exit(1);
    }

    DBGH(1) << "Recovery completed" << std::endl;
  }
}

std::string cxlbuf::PmemFile::get_backing_fname() const {
    return this->path.string() + ".cxlbuf_backing";
}

std::string cxlbuf::PmemFile::get_dependency_fname() const {
  return this->path.string() + "-dependencies.txt";
}

bool cxlbuf::PmemFile::has_backing_file() {
  const auto bfile = fs::path(this->get_backing_fname());
  DBGH(3) << "Checking if backing file " << bfile << " exists" << std::endl;
  return std::filesystem::is_regular_file(bfile);
}

void cxlbuf::PmemFile::create_backing_file_internal() {
  const auto bfname = this->get_backing_fname();

  /* Check if original file size if 0, in that case allocate the file of the
   * same size as the mapping.
   *
   * TODO: This should be done by intercepting calls to fallocate and friends
   */
  if (fs::file_size(this->path) == 0) {
    DBGH(2) << "Creating backing file by allocating " << this->len << " bytes"
            << std::endl;
    
    int fd = open(bfname.c_str(), O_CREAT | O_RDWR, 0666);
    if (fd == -1) {
      DBGE << "Unable to open file descriptor for the backing file" << std::endl;
      DBGE << PSTR();
      exit(1);
    }

    NVSL_ASSERT(lseek(fd, this->len + 1, SEEK_SET) == (off_t)(this->len + 1),
                "lseek for backing file failed");
    NVSL_ASSERT(1 == write(fd, "\0", 1), "write to backing file failed");
    close(fd);

    DBGH(3) << "New backing file size: "
            << fs::file_size(this->get_backing_fname()) << std::endl;
  } else {
    DBGH(2) << "Creating backing file of size by copying...";
    fs::copy_file(this->path, bfname);
    DBG << "done" << std::endl;
  }
}

void cxlbuf::PmemFile::create_backing_file() {
  if (not this->has_backing_file()) {
    /* Only copy to the backing file if this is the first time */
    this->create_backing_file_internal();
  } else {
    DBGH(1) << "Recreating " << this->path << " from "
            << this->get_backing_fname() << std::endl;

    int src_fd = open(this->get_backing_fname().c_str(), O_RDONLY);
    if (src_fd == -1) {
      DBGE << "Unable to open the backing file for copy" << std::endl;
      DBGE << PSTR() << std::endl;
      exit(1);
    }

    int dst_fd = open(this->path.c_str(), O_WRONLY);
    if (dst_fd == -1) {
      DBGE << "Unable to open the original for copy" << std::endl;
      DBGE << PSTR() << std::endl;
      exit(1);
    }

    int sf_ret = sendfile(dst_fd, src_fd, 0, this->len);

    if (sf_ret == -1) {
      DBGE << "Sendfile for copying from backing file failed" << std::endl;
      DBGE << PSTR() << std::endl;
      exit(1);
    }

    close(src_fd);
    close(dst_fd);
  }
}

void cxlbuf::PmemFile::map_backing_file() {
  const auto bfname = this->get_backing_fname();

  /* Since this file is opened in the PM, create a corresponding backing file
     and map it at +0x10000000000 */
  void *bck_addr = (char *)this->addr + 0x10000000000;
  const int bck_fd = open(bfname.c_str(), O_RDWR);

  if (bck_fd == -1) {
    perror("Unable to open backing file");
    exit(1);
  }

  int bck_flags = 0;
  bck_flags |= MAP_SHARED_VALIDATE;
  bck_flags |= MAP_SYNC;

  DBGH(3) << "Calling mmap for backing: "
          << mmap_to_str(bck_addr, this->len, PROT_READ | PROT_WRITE, bck_flags,
                         bck_fd, 0)
          << std::endl;
  void *mbck_addr = MAP_FAILED;

  if (is_prefix("/mnt/cxl0", this->get_backing_fname())) {
    void *tmp_addr = real_mmap(nullptr, this->len, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE, bck_fd, 0);
    if (tmp_addr == MAP_FAILED) {
      DBGE << "Unable to map cxl0 file" << std::endl;
      DBGE << PSTR() << "\n";
      exit(1);
    }
    mbck_addr = nvsl::libvram::malloc(this->len);

    /* Set the backing_file_start to let the logging mechanism copy content into
       the backing file on snapshot() */
    NVSL_ASSERT(nvsl::cxlbuf::backing_file_start == nullptr,
                "Mapping multiple backing files not supported");
    nvsl::cxlbuf::backing_file_start = mbck_addr;

    /* Copy the content from the file to the buffer */
    memcpy(mbck_addr, tmp_addr, this->len);

    /* We no longer need the temporary mapping, the mapping on the GPU will
     * behave as the backing region now */
    real_munmap(tmp_addr, this->len);
  } else if (is_prefix("/mnt/pmem0/", this->get_backing_fname())) {
    nvsl::cxlbuf::backing_file_start = RCast<void *>(0x20000000000);
    mbck_addr = real_mmap(bck_addr, this->len, PROT_READ | PROT_WRITE,
                          MAP_SHARED_VALIDATE | MAP_SYNC, bck_fd, 0);
  }

  if (mbck_addr == MAP_FAILED) {
    DBGE << "Unable to map backing file" << std::endl;
    DBGE << PSTR() << std::endl;
    exit(1);
  } else {
    DBGH(2) << "Backing file " << bfname << " for " << fs::path(this->path)
            << " mapped at address " << mbck_addr << std::endl;
  }

  DBGH(4) << "Trying to write to the backing file...";
  *(char *)mbck_addr = 1;
  DBGH(4) << "done" << std::endl;
}

cxlbuf::PmemFile::PmemFile(const fs::path &path, void *addr, size_t len)
    : path(path), addr(addr), len(len) {
  /* Only check for recovery if this is a non-anonymous mapping */
  if (not path.empty()) {
    if (const auto recovery_files = this->needs_recovery();
        not recovery_files.empty()) {
      recover(recovery_files);
    }
  }
}

void cxlbuf::PmemFile::set_addr(void *addr) {
  this->addr = addr;
}

void cxlbuf::PmemFile::write_dependency_internal() {
  const int fd = open(this->get_dependency_fname().c_str(),
                      O_CREAT | O_APPEND | O_RDWR, 0666);

  if (fd == -1) {
    perror("Unable to open/create dependency file");
    exit(1);
  }

  const int pid = getpid();
  const pthread_t tid = pthread_self();

  const auto entry =
      S(pid) + "," + S(tid) + "," + S((uint64_t)this->addr) + "\n";

  const auto bytes = write(fd, entry.c_str(), strlen(entry.c_str()));

  if ((size_t)bytes != strlen(entry.c_str())) {
    perror("Unable to write to the dependency file");
    exit(1);
  }

  close(fd);
}

void cxlbuf::PmemFile::write_dependency() {
  if (this->get_dependency_fname() != "-dependencies.txt") {
    this->write_dependency_internal();
  }
}

void *cxlbuf::PmemFile::map_to_page_cache(int flags, int prot, int fd,
                                          off_t off) {
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

  if (result == MAP_FAILED) {
    DBGW << "Call to mmap failed" << std::endl;
  } else if (fd != -1) {
    const cxlbuf::addr_range_t range = {
        (size_t)(this->addr), (size_t)((char *)this->addr + this->len)};
    const cxlbuf::fd_metadata_t fd_metadata = {
        .fd = fd, .range = range, .fpath = this->path};

    cxlbuf::mapped_addr.insert_or_assign(fd, fd_metadata);
    DBGH(2) << "mmap_addr recorded " << fd << " -> " << (void *)range.start
            << ", " << (void *)range.end << std::endl;

    if (not addr_in_range(result) and this->addr != nullptr) {
      DBGE << "Cannot map pmem file in range" << std::endl;
      DBGE << "Asked " << this->addr << ", got " << result << std::endl;
      perror("mmap:");
      system(("cat /proc/" + std::to_string(getpid()) + "/maps >&2").c_str());
      system(("ls -lah /proc/" + std::to_string(getpid()) + "/fd >&2").c_str());
      exit(1);
    }
  }

  /* Add an entry for this process to the dependency file. This will allow us
     to locate the logs when the file is openede after a crash */
  if (fd != -1) {
    this->write_dependency();
  }

  return result;
}
