// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   pfmonitor.cc
 * @date   novembre  9, 2022
 * @brief  Pointers page fault for the current process
 */

#include <cstring>
#include <functional>
#include <iostream>

#include <linux/userfaultfd.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/types.h>

#include "libcxlfs/numabinder.hh"
#include "libcxlfs/pfmonitor.hh"
#include "nvsl/common.hh"
#include "nvsl/error.hh"
#include "nvsl/trace.hh"

using nvsl::P;

int userfaultfd(int flags) {
  return syscall(SYS_userfaultfd, flags);
}

int PFMonitor::error(std::string err_str, bool has_errno /* = true */) {
  if (has_errno) {
    DBGE << PSTR() << ": " << err_str << std::endl;
  } else {
    DBGE << err_str << std::endl;
  }

  return -1;
}

static PFMonitor *pfm_static;

void PFMonitor::handler(int sig, siginfo_t *si, void *ucontext) {
  int rc = -1;
  const void *place = si->si_addr;
  const auto place_u64 = nvsl::RCast<uint64_t>(place);
  const auto start_u64 = nvsl::RCast<uint64_t>(pfm_static->start);
  const auto end_u64 = start_u64 + pfm_static->len;

  if ((place_u64 >= start_u64) and (place_u64 < end_u64)) {
    const auto page_aligned = ((uint64_t)place / 4096) * 4096;
    auto src = pfm_static->cb(page_aligned);

    rc = mprotect((void *)page_aligned, 0x1000, PROT_READ | PROT_WRITE);
    if (rc == -1) {
      DBGE << "mprotect failed: " << PSTR() << "\n";
      exit(1);
    }

    memcpy((void *)page_aligned, src, 0x1000);
    free(src);
  } else {
    std::cerr << "Unhandled SIGSEGV at " << place << "\n";
    std::cerr << nvsl::get_stack_trace() << "\n";
    exit(1);
  }

  return;
}

/** @brief Monitor fd for page faults. Blocking. **/
int PFMonitor::monitor_fd_blocking(int fd, Callback &cb) {
  int rc = -1;

  std::cerr << "Registering sigaction handler\n";

  struct sigaction action;
  action.sa_sigaction = handler;
  sigemptyset(&action.sa_mask);
  action.sa_flags = SA_SIGINFO;
  rc = sigaction(SIGSEGV, &action, NULL);
  this->cb = cb;

  return rc;
}

  /** @brief Get the pagefault file descriptor **/
int PFMonitor::get_pf_fd() {
  int pf_fd = -1;

  auto error = [](std::string err_str, bool has_errno = true) {
    DBGE << PSTR() << ": " << err_str << std::endl;
    return -1;
  };

  /* Open a userfaultd file descriptor */
  if ((pf_fd = userfaultfd(O_NONBLOCK)) == -1) {
    return error(": ++ userfaultfd failed");
  }

  /* Check if the kernel supports the read/POLLIN protocol */
  struct uffdio_api uapi = {};
  uapi.api = UFFD_API;
  if (ioctl(pf_fd, UFFDIO_API, &uapi)) {
    return error(": ++ ioctl(fd, UFFDIO_API, ...) failed");
  }

  if (uapi.api != UFFD_API) {
    return error(": ++ unexepcted UFFD api version.");
  }

  return pf_fd;
}

/** @brief Initialize the internal state **/
int PFMonitor::init(int tgt_node /* = 0 */) {
  this->pf_fd = get_pf_fd();
  this->tgt_node = tgt_node;

  return this->pf_fd;
}

  /** @brief Monitor for page faults and handle them using cb **/
  int PFMonitor::monitor(Callback cb) {
    pfm_static = this;
    int res = this->monitor_fd_blocking(pf_fd, cb);

    return res;
  }

  /**
   * @brief Register an address range with PFMonitor
   * @param[in] start Starting address of the range to monitor
   * @param[in] len Length of the range in bytes
   *
   * @return 0 if range was registered successfuly. Underlying ioctl() return
   * code otherwise.
   *
   * @details Address start and start+size should be page aligned.
   **/
  int PFMonitor::register_range(void *start, size_t len) {
    this->start = start;
    this->len = len;

    return 0;
  }
