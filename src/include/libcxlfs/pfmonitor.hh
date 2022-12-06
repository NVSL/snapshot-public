// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   pfmonitor.hh
 * @date   novembre 13, 2022
 * @brief  Brief description here
 */

#pragma once

#include <bits/types/siginfo_t.h>
#include <functional>
#include <string>

#include <sys/types.h>

class PFMonitor {
public:
  using addr_t = uint64_t;
  using Callback = std::function<void *(addr_t)>;
private:
  int pf_fd = -1;
  int tgt_node = 0;

  int error(std::string err_str, bool has_errno = true);

  /** @brief Monitor fd for page faults. Blocking. **/
  int monitor_fd_blocking(int fd, Callback &cb);

  /** @brief Get the pagefault file descriptor **/
  int get_pf_fd();

public:
  PFMonitor() {}

  /**
   * @brief Initialize the internal state
   * @param tgt_node[in] Numa node to move pages to on allocation
   **/

  int init(int tgt_node = 0);

  /** @brief Monitor for page faults and handle them using cb **/
  int monitor(Callback cb);

  /** @brief Register an address range with PFMonitor **/
  int register_range(void *start, size_t range);

  static void handler(int sig, siginfo_t *si, void *ucontext);

private:
  Callback cb;
  void *start;
  size_t len;
};
