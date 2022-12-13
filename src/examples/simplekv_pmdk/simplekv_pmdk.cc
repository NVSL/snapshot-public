// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * simplekv.cpp -- implementation of simple kv which uses vector to hold
 * values, string as a key and array to hold buckets
 */

#include "libcxlfs/libcxlfs.hh"
#include "nvsl/clock.hh"
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <libpmemobj++/make_persistent_atomic.hpp>
#include <libpmemobj++/pool.hpp>
#include <stdexcept>
#include <vector>

#include "simplekv_pmdk.hh"

constexpr size_t POOL_SZ = 4UL * 1024 * 1024 * 1024;

using kv_type = simple_kv<int, 3500>;

struct root {
  pmem::obj::persistent_ptr<kv_type> kv;
};

void show_usage(char *argv[]) {
  std::cerr << "usage: " << argv[0] << " file-name [get key|put key value]"
            << std::endl;
}

typedef void(fptr_t)(size_t);

void resize_cache(size_t pages) {
  void *handle = dlopen(NULL, RTLD_NOW);
  if (handle) {
    fptr_t *resize_cache_sym = (fptr_t *)dlsym(handle, "resize_cache");
    if (resize_cache_sym) {
      std::cerr << "Calling resize cache\n";
      resize_cache_sym(pages);
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    show_usage(argv);
    return 1;
  }

  const char *path = argv[1];

  pmem::obj::pool<root> pop;
  try {
    if (not std::filesystem::is_regular_file(path)) {
      pop = pmem::obj::pool<root>::create(path, "simplekv", POOL_SZ);
      if (pop.root() == nullptr) {
        std::cerr << "Pool creation failed\n";
        exit(1);
      }
    } else {
      pop = pmem::obj::pool<root>::open(path, "simplekv");
    }
    auto r = pop.root();

    if (r->kv == nullptr) {
      pmem::obj::transaction::run(
          pop, [&] { r->kv = pmem::obj::make_persistent<kv_type>(); });
    }

    if (std::string(argv[2]) == "get" && argc == 4)
      std::cout << r->kv->get(argv[3]) << std::endl;
    else if (std::string(argv[2]) == "put" && argc == 5)
      r->kv->put(argv[3], std::stoi(argv[4]));
    else if (std::string(argv[2]) == "burst" && std::string(argv[3]) == "get" &&
             argc == 5) {
      int m = std::stoi(argv[4]);
      for (int i = 0; i < m; i++) {
        char key[32] = {0};
        sprintf(key, "key%d", i);
        r->kv->get(key);
      }
    } else if (std::string(argv[2]) == "burst" &&
               std::string(argv[3]) == "put" && argc == 5) {
      nvsl::Clock clk;
      clk.tick();
      int m = std::stoi(argv[4]);
      for (int i = 0; i < m; i++) {
        char key[32] = {0};
        sprintf(key, "key%d", i);
        r->kv->put(key, i);
      }
      clk.tock();
      std::cout << clk.summarize() << std::endl;
    } else if (std::string(argv[2]) == "ycsb" && argc == 5) {
      std::string line;
      std::ifstream load_workload(argv[3]);
      std::ifstream run_workload(argv[4]);

      std::vector<std::string> load_keys;
      std::vector<std::string> run_ops;
      std::vector<std::string> run_keys;
      nvsl::Clock clk;
      int value = 4455;

      std::cout << "Reading load trace" << std::endl;
      if (load_workload.is_open()) {
        while (getline(load_workload, line)) {
          // std::cout << line << '\n';
          std::string delimiter = " ";
          // load_keys.push_back(line.substr(0, line.find(delimiter)));
          load_keys.push_back(line.erase(0, line.find(delimiter) + 1));
          // std::cout << load_keys[load_keys.size() - 1] << '\n';
        }
        load_workload.close();
      }

      std::cout << "Reading run trace" << std::endl;
      if (run_workload.is_open()) {
        while (getline(run_workload, line)) {
          // std::cout << line << '\n';
          std::string delimiter = " ";
          run_ops.push_back(line.substr(0, line.find(delimiter)));
          run_keys.push_back(line.erase(0, line.find(delimiter) + 1));
          // std::cout << run_ops[run_ops.size() - 1] << ' ';
          // std::cout << run_keys[run_keys.size() - 1] << '\n';
        }
        run_workload.close();
      }

      resize_cache(nvsl::libcxlfs::MEM_SIZE >> 12);

      int c = 0;
      std::cout << "Executing load trace" << std::endl;
      for (auto &key : load_keys) {
        r->kv->put(key, value);
        // std::cout << c++ << "\n";
      }

      resize_cache(nvsl::libcxlfs::CACHE_SIZE >> 12);

      size_t run_size = run_ops.size();
      std::cout << "Executing run trace" << std::endl;
      clk.tick();
      volatile size_t result = 0;
      for (size_t run = 0; run < 1; run++) {
        for (size_t i = 0; i < run_size; ++i) {
          if (run_ops[i] == "Update")
            r->kv->put(run_keys[i], value);
          else if (run_ops[i] == "Read") {
            result = result + r->kv->get(run_keys[i]);
          }
        }
      }
      clk.tock();

      clk.reconcile();

      std::cout << clk.summarize(run_ops.size()) << std::endl;
      exit(0);
    } else {
      show_usage(argv);
      pop.close();
      return 1;
    }
  } catch (pmem::pool_error &e) {
    std::cerr << e.what() << std::endl;
    std::cerr << "To create pool run: pmempool create obj "
                 "--layout=simplekv -s 100M path_to_pool"
              << std::endl;
  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  try {
    pop.close();
  } catch (const std::logic_error &e) {
    std::cerr << e.what() << std::endl;
  }
  return 0;
}
