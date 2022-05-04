// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   run.cc
 * @date   avril 10, 2022
 * @brief  Entry point for all benchmarks
 */

#include "nvsl/string.hh"
#include "run.hh"

#include <cstdlib>
#include <functional>
#include <map>
#include <string>
#include <vector>

using namespace nvsl;

int main(int argc, char *argv[]) {
  const std::map<std::string, std::function<void(void)>> msb = {
      std::make_pair("clwbvsntstore",
                     std::function<void(void)>(mb_clwbvsntstore)),
      std::make_pair("clwbsfencedist",
                     std::function<void(void)>(mb_clwbsfencedist)),
  };

  for (int i = 1; i < argc; i++) {
    const auto arg = S(argv[i]);
    if (is_prefix("--run", arg)) {
      const auto kv = split(arg, "=", 2);
      const auto workloads = split(kv[1], ",");

      for (const auto &workload : workloads) {
        try {
          msb.at(workload)();
        } catch (const std::exception &e) {
          DBGE << "Workload `" << workload << "' not found.\n";
          exit(EXIT_FAILURE);
        }
      }

      exit(EXIT_SUCCESS);
    }
  }

  DBGE << "Usage:\n";
  DBGE << "\t" << argv[0] << " --run=<comma separated benchmark names>\n";
  DBGE << "\n";
  DBGE << "Available benchmarks:\n";
  for (const auto &[name, _] : msb) {
    DBGE << "\t" << name << "\n";
  }

  exit(EXIT_FAILURE);
}
