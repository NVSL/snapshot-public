// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   pfmonitor.cc
 * @date   novembre  9, 2022
 * @brief  Pointers page fault for the current process
 */

#include <functional>
#include <iostream>

#include <linux/userfaultfd.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/types.h>

#include "libcxlfs/pfmonitor.hh"
#include "nvsl/common.hh"
#include "nvsl/error.hh"

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

/** @brief Monitor fd for page faults. Blocking. **/
int PFMonitor::monitor_fd_blocking(int fd, Callback &cb) {
  struct pollfd evt = {};
  struct uffd_msg fault_msg = {0};

  evt.fd = fd;
  evt.events = POLLIN;

  while (1) {
    int pollval = poll(&evt, 1, 10);

    switch (pollval) {
    case -1:
      perror("poll/userfaultfd");
      continue;
    case 0:
      continue;
    case 1:
      DBGH(2) << "Handling page fault!" << std::endl;
      break;
    default:
      DBGE << "Unexpected poll result" << std::endl;
      return -1;
    }

    /* unexpected poll events */
    if (evt.revents & POLLERR) {
      return error("++ POLLERR");
    } else if (evt.revents & POLLHUP) {
      return error("++ POLLHUP");
    }

    if (read(fd, &fault_msg, sizeof(fault_msg)) != sizeof(fault_msg)) {
      return error("ioctl_userfaultfd read");
    }

    char *place = (char *)fault_msg.arg.pagefault.address;
    DBGH(3) << "Got page fault at " << (void *)(place) << std::endl;

    /* handle the page fault */
    if (fault_msg.event & UFFD_EVENT_PAGEFAULT) {
      struct uffdio_range range = {};
      auto page_aligned = ((uint64_t)place / 4096) * 4096;
      range.start = page_aligned;
      range.len = 4096;

      cb(range.start);

      if (ioctl(fd, UFFDIO_WAKE, &range) == -1) {
        perror("ioctl/wake");
        exit(1);
      } else {
        std::cerr << "fault handled\n";
      }
    }
  }
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
int PFMonitor::init() {
  this->pf_fd = get_pf_fd();

  return this->pf_fd;
}

  /** @brief Monitor for page faults and handle them using cb **/
  int PFMonitor::monitor(Callback cb) {
    int res = 0;

    NVSL_ASSERT(pf_fd != -1, "init() not called for PFMonitor");

    if (pf_fd != -1) {
      res = this->monitor_fd_blocking(pf_fd, cb);
      close(pf_fd);
    } else {
      res = pf_fd;
    }

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
    struct uffdio_register reg = {};
    int res;

    NVSL_ASSERT(pf_fd != -1, "init() not called for PFMonitor");

    DBGH(3) << "Register range start " << start << " len " << len << "\n";

    reg.mode = UFFDIO_REGISTER_MODE_MISSING;
    reg.range = {};
    reg.range.start = (unsigned long long)start;
    reg.range.len = (unsigned long long)len;

    res = ioctl(this->pf_fd, UFFDIO_REGISTER, &reg);
    if (res != 0) {
      return error("ioctl(fd, UFFDIO_REGISTER, ...) failed");
    }

    DBGH(2) << "Range registered\n";

    return 0;
  }
