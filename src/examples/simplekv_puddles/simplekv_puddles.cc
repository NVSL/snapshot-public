// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * simplekv.cpp -- implementation of simple kv which uses vector to hold
 * values, string as a key and array to hold buckets
 */

#include <fstream>
#include <stdexcept>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "common.hh"
#include "libstoreinst.h"

#include "simplekv_puddles.hh"

constexpr size_t MIN_POOL_SZ = (2 * 1024 * 1024);
size_t cur_ator_head = 0;

using kv_type = simple_kv<int, 10000>;

void *alloc(void *region, size_t bytes) {
  cur_ator_head += bytes;
  return (char*)region + bytes;
}

void show_usage(char *argv[]) {
  std::cerr << "USAGE: " << std::endl
            << "\t" << argv[0] << " puddle-name (get key | put key value)\n"
            << "\t" << argv[0]
            << " puddle-name burst (get number | put number)\n"
            << "\t" << argv[0]
            << " puddle-name ycsb path_to_load_workload path_to_run_workload\n";
}

int main(int argc, char *argv[]) {
  std::cout << "---" << std::endl;

  if (argc < 3) {
    show_usage(argv);
    return 1;
  }

  const char *path = argv[1];

  try {
    auto fd = open(path, O_RDWR);
    if (fd == -1) {
      perror("open failed");
      exit(1);
    }

    auto start_addr_str = get_env_str("PMEM_START_ADDR");
    auto start_addr = (void*)std::stoull(start_addr_str, 0, 16);
    auto res = mmap(start_addr, MIN_POOL_SZ * 10, PROT_READ | PROT_WRITE, MAP_SHARED_VALIDATE | MAP_FIXED, fd, 0);

    std::cout << "mounted at " << (void*)res << std::endl;

    if (res == (void*)-1) {
      perror("mmap failed");
      exit(1);
    }

    auto root = reinterpret_cast<kv_type*>(res);

    std::cout << "Root at " << (void*)root << std::endl;

    root->init(root, MIN_POOL_SZ*10);
    if (root == nullptr) {
        msync(root, MIN_POOL_SZ*10, MS_SYNC);
    }

    startTracking = true;

    if (std::string(argv[2]) == "get" && argc == 4) {
      std::cout << root->get(argv[3]) << std::endl;
    } else if (std::string(argv[2]) == "put" && argc == 5) {
      root->put(argv[3], std::stoi(argv[4]));
    } else if (std::string(argv[2]) == "burst" &&
               std::string(argv[3]) == "get" && argc == 5) {

      // puddles::Clock clk;
      // clk.tick();
      int m = std::stoi(argv[4]);
      for (int i = 0; i < m; i++) {
        char key[32] = {0};
        sprintf(key, "key%d", i);
        std::cout << root->get(key) << std::endl;
      }
      // clk.tock();
      // std::cout << clk.summarize() << std::endl;
    } else if (std::string(argv[2]) == "burst" &&
               std::string(argv[3]) == "put" && argc == 5) {

      // puddles::Clock clk;
      // clk.tick();
      int m = std::stoi(argv[4]);
      for (int i = 0; i < m; i++) {
        char key[32] = {0};
        sprintf(key, "key%d", i);
        root->put(key, i);
      }
      // clk.tock();
      // std::cout << clk.summarize() << std::endl;
    } else if (std::string(argv[2]) == "ycsb" && argc == 5) {
      std::string line;
      std::ifstream load_workload(argv[3]);
      std::ifstream run_workload(argv[4]);

      std::vector<std::string> load_keys;
      std::vector<std::string> run_ops;
      std::vector<std::string> run_keys;
      // Clock clk;

      int value = 4455;

      /* Read the load and run workload traces */
      std::cout << "Reading load trace" << std::endl;
      if (load_workload.is_open()) {
        while (getline(load_workload, line)) {
          std::string delimiter = " ";
          load_keys.push_back(line.erase(0, line.find(delimiter) + 1));
        }
        load_workload.close();
      }

      std::cout << "Reading run trace" << std::endl;
      if (run_workload.is_open()) {
        while (getline(run_workload, line)) {
          std::string delimiter = " ";
          run_ops.push_back(line.substr(0, line.find(delimiter)));
          run_keys.push_back(line.erase(0, line.find(delimiter) + 1));
        }
        run_workload.close();
      }

      /* Execute the load trace */
      std::cout << "| Executing load trace" << std::endl;
      size_t iter = 0;
      for (auto &key : load_keys) {
        if (iter++%100000 == 0) {
          std::cout << (iter*100)/load_keys.size() << "%...";
          std::flush(std::cout);
        }
        root->put(key, value);
        snapshot(root, MIN_POOL_SZ*10, MS_SYNC);
      }
      std::cout << std::endl;

      /* Execute the run trace */
      std::cout << "Executing run trace" << std::endl;
      size_t run_size = run_ops.size();
      // clk.tick();
      volatile size_t result = 0;
      for (size_t run = 0; run < 1; run++) {
        for (size_t i = 0; i < run_size; ++i) {
          if (run_ops[i] == "Update") {
            root->put(run_keys[i], value);
            snapshot(root, MIN_POOL_SZ*10, MS_SYNC);
          } else if (run_ops[i] == "Read") {
            result = result + root->get(run_keys[i]);
          }
        }
      }
    } else {
      show_usage(argv);
      return 1;
    }
  } catch (std::exception &e) {
    std::cerr << "Got an exception: " << e.what() << std::endl;
  }

  return 0;
}
