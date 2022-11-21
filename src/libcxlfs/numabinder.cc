// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   membwdist.cc
 * @date   novembre 14, 2022
 * @brief  Brief description here
 */

#include "nvsl/common.hh"
#include "nvsl/error.hh"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <numa.h>
#include <unistd.h>
#include <utmpx.h>

#include "libcxlfs/numabinder.hh"

NumaBinder::node_map_t NumaBinder::get_numa_map() {
  node_map_t result = {};

  long nproc = sysconf(_SC_NPROCESSORS_CONF);

  for (cpuid_t cpuid = 0; cpuid < nproc; cpuid++) {
    const auto node = numa_node_of_cpu(cpuid);

    DBGH(4) << "cpuid " << cpuid << " node " << node << std::endl;
    result[cpuid] = node;
  }

  return result;
}

/* Build a list of processes bound to specific cores. Returns -1 if nothing
   can be found. Assumes an upper bound of 4k CPUs. */
/* INFO: From afl-fuzz.c: https://github.com/google/AFL/blob/master/afl-fuzz.c
 */

int NumaBinder::bind_to_free_cpu(int cpu_to_bind /* = -1 */) {
  DIR *d;
  struct dirent *de;
  cpu_set_t c;

  uint8_t cpu_used[4096] = {0};
  int32_t i;

  int cpu_core_count = sysconf(_SC_NPROCESSORS_ONLN);

  if (cpu_core_count < 2) return -1;

  d = opendir("/proc");

  if (!d) {
    DBGW << "Unable to access /proc - can't scan for free CPU cores.";
    return -1;
  }

  DBGH(3) << "Checking CPU core loadout..." << std::endl;

  /* Scan all /proc/<pid>/status entries, checking for Cpus_allowed_list.
          Flag all processes bound to a specific CPU using cpu_used[]. This will
               fail for some exotic binding setups, but is likely good enough in
     almost all real-world use cases. */

  while ((de = readdir(d))) {

    FILE *f;
    char tmp[MAX_LINE];
    uint8_t has_vmsize = 0;

    if (!isdigit(de->d_name[0])) continue;

    auto fn_str = ("/proc/" + std::string(de->d_name) + "/status");

    if (!(f = fopen(fn_str.c_str(), "r"))) {
      continue;
    }

    while (fgets(tmp, MAX_LINE, f)) {

      uint32_t hval;

      /* Processes without VmSize are probably kernel tasks. */

      if (!strncmp(tmp, "VmSize:\t", 8)) has_vmsize = 1;

      if (!strncmp(tmp, "Cpus_allowed_list:\t", 19) && !strchr(tmp, '-') &&
          !strchr(tmp, ',') && sscanf(tmp + 19, "%u", &hval) == 1 &&
          hval < sizeof(cpu_used) && has_vmsize) {

        cpu_used[hval] = 1;
        break;
      }
    }

    fclose(f);
  }

  closedir(d);
  if (cpu_to_bind != -1) {

    if (cpu_to_bind >= cpu_core_count) {
      DBGE << "The CPU core id to bind should be between 0 and"
           << cpu_core_count - 1 << std::endl;
      return -1;
    }

    if (cpu_used[cpu_to_bind]) {
      DBGE << "The CPU core " << cpu_to_bind << " to bind is not free!"
           << std::endl;
      return -1;
    }

    i = cpu_to_bind;

  } else {

    for (i = 0; i < cpu_core_count; i++)
      if (!cpu_used[i]) break;
  }

  if (i == cpu_core_count) {
    DBGE << "No more free CPU cores" << std::endl;
    return -1;
  }

  DBGH(1) << "Found a free CPU core, binding to " << i << std::endl;

  cpu_aff = i;

  CPU_ZERO(&c);
  CPU_SET(i, &c);

  if (sched_setaffinity(0, sizeof(c), &c)) {
    DBGE << "sched_setaffinity failed: " << PSTR() << std::endl;
    return -1;
  }

  return i;
}

void NumaBinder::bind_to_node(int node_id) {
  const int cpu = sched_getcpu();
  const int node = numa_node_of_cpu(cpu);

  (void)get_numa_map();

  DBGH(1) << "Current cpu " << cpu << " node " << node << std::endl;
}
