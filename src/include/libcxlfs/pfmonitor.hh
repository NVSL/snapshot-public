// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   pfmonitor.hh
 * @date   novembre 13, 2022
 * @brief  Brief description here
 */

#pragma once

#include <functional>
#include <string>

class PFMonitor {
public:
  using addr_t = uint64_t;
  using Callback = std::function<void(addr_t)>;

private:
  int pf_fd = -1;

  int error(std::string err_str, bool has_errno = true);

  /** @brief Monitor fd for page faults. Blocking. **/
  int monitor_fd_blocking(int fd, Callback &cb);

  /** @brief Get the pagefault file descriptor **/
  int get_pf_fd();

public:
  PFMonitor() {}

  /** @brief Initialize the internal state **/
  int init();

  /** @brief Monitor for page faults and handle them using cb **/
  int monitor(Callback cb);

  /** @brief Register an address range with PFMonitor **/
  int register_range(void *start, size_t range);
};
