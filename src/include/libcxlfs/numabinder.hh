// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   numabinder.hh
 * @date   novembre 21, 2022
 * @brief  Brief description here
 */

#pragma once

#include <cstdint>
#include <map>
#include <unistd.h>

class NumaBinder {
private:
  using cpuid_t = long;
  using nodeid_t = long;
  using node_map_t = std::map<cpuid_t, nodeid_t>;

  constexpr static uint64_t MAX_LINE = 8192;

  int cpu_aff = -1;
  node_map_t get_numa_map();

public:
  int bind_to_node(int node_id);
  void mmap_from_node(void *addr, size_t length, int prot, int flags, int fd,
                      off_t offset, int node);
  int bind_to_free_cpu(int cpu_to_bind = -1);
  static int get_cur_numa_node();
};
