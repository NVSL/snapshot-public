// -*- mode: c++-mode; c-basic-offset: 2; -*-

/**
 * @file   map.cc
 * @date   December 22, 2020
 * @brief  Brief description here
 */

#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <boost/interprocess/indexes/iset_index.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/mem_algo/rbtree_best_fit.hpp>
#include <boost/interprocess/mem_algo/simple_seq_fit.hpp>
#include <boost/interprocess/sync/mutex_family.hpp>

#include "map.hh"
#include "map_btree.hh"
#include "nvsl/clock.hh"

constexpr size_t MIN_POOL_SZ = 1024 * 1024UL * 1024;

using namespace std::chrono;
using namespace nvsl;

extern bool startTracking;

mmf *tls_res;

void print_help() {
  // clang-format off
  std::cout << "Help: " << std::endl;
  std::cout << "\ti <int>  : Insert a new value" << std::endl;
  std::cout << "\tr <int>  : Remove a value" << std::endl;
  std::cout << "\tp        : Prints all the values to stdout" << std::endl;
  std::cout << "\tc <int>  : Lookup the value in the map" << std::endl;
  // clang-format on
}

struct args_t {
  std::string puddle_path;
  map_backend_t map_backend;

  bool bulk;
  std::string bulk_op = "";
  size_t bulk_cnt;
};

void print_usage(std::string bin_name) {
  std::cerr << "Usage: \n\t" << bin_name
            << " <puddle> <backend> [bulk <op> <cnt>]\n"
            << "\nBackends:"
            << "\n\t btree" << std::endl;
}

args_t parse_args(std::vector<std::string> args_v) {
  if ((args_v.size() != 3 && args_v.size() != 6) ||
      (args_v.size() == 6 && args_v[3] != "bulk")) {
    print_usage(args_v[0]);
    exit(1);
  }

  args_t args = {};
  args.puddle_path = args_v[1];

  if (args_v[2] == "btree") {
    args.map_backend = map_backend_t::BTREE;
  } else {
    std::cerr << "ERROR: Wrong backend" << std::endl;
    print_usage(args_v[0]);
    exit(1);
  }

  if (args_v.size() == 6) {
    args.bulk = true;
    args.bulk_op = args_v[4];

    try {
      args.bulk_cnt = std::stoull(args_v[5], 0, 10);
    } catch (std::exception &e) {
      std::cerr << "Error parsing args: " << e.what() << std::endl;
    }
  }

  return args;
}

std::vector<std::string> split(const std::string &text, char sep) {
  std::vector<std::string> tokens;
  std::size_t start = 0, end = 0;
  while ((end = text.find(sep, start)) != std::string::npos) {
    tokens.push_back(text.substr(start, end - start));
    start = end + 1;
  }
  tokens.push_back(text.substr(start));
  return tokens;
}

void perform_bulk_ops(mmf *res, const args_t &args, MapRoot *root_ptr) {
  nvsl::Clock clk;
  switch (args.bulk_op[0]) {
  case 'i':
    clk.tick();
    for (size_t i = 0; i < args.bulk_cnt; i++) {
      root_ptr->mapops->insert(root_ptr, rand(), 0);
      msync(res->get_address(), res->get_size(), MS_SYNC);
    }
    clk.reconcile();
    clk.tock();
    break;
  case 'r':
    {
      std::vector<uint64_t> keys;
      keys.reserve(args.bulk_cnt);

      for (size_t i = 0; i < args.bulk_cnt; i++) {
        keys.push_back(rand());
        root_ptr->mapops->insert(root_ptr, keys.back(), 0);
        msync(res->get_address(), res->get_size(), MS_SYNC);
      }

      std::cout << "Allocated all nodes" << std::endl;

      clk.tick();
      for (size_t i = args.bulk_cnt - 1; i > 0; i--) {
        root_ptr->mapops->remove(root_ptr, keys[i]);
        msync(res->get_address(), res->get_size(), MS_SYNC);
      }
      clk.tock();
    }
    break;
  case 'c':
    {
      std::vector<uint64_t> keys;
      keys.reserve(args.bulk_cnt);

      for (size_t i = 0; i < args.bulk_cnt; i++) {
        keys.push_back(rand());
        root_ptr->mapops->insert(root_ptr, keys.back(), 0);
      }

      clk.tick();
      for (size_t i = 0; i < args.bulk_cnt; i++) {
        root_ptr->mapops->lookup(root_ptr, keys[i]);
      }
      clk.reconcile();
      clk.tock();
    }
    break;
  default:
    std::cout << "operation " << args.bulk_op[0] << " is not implemented"
              << std::endl;
    break;
  }

  clk.reconcile();
  std::cout << clk.summarize(args.bulk_cnt) << std::endl;
}
int main(int argc, char *argv[]) {
  args_t args = parse_args(std::vector<std::string>(argv, argv + argc));

  auto *res =
      new mmf(bip::open_or_create, args.puddle_path.c_str(), MIN_POOL_SZ);
  tls_res = res;

  std::cerr << "reservoir addr = " << (void *)res->get_address() << std::endl;

  auto *root_ptr = res->find<MapRoot>("root").first;

  if (root_ptr == NULL) {
    std::cout << "Allocating root" << std::endl;
    // root_ptr = safe_malloc<MapRoot>(res);
    root_ptr = res->construct<MapRoot>("root")();
    if (root_ptr == NULL) {
      throw std::runtime_error("Allocating root failed");
    }

    // memset(root_ptr, 0, sizeof(*root_ptr));
  } else {
    std::cout << "Got previous root" << std::endl;
  }

  if (args.map_backend == map_backend_t::BTREE) {
    root_ptr->mapops = map_btree(res);
  }

  size_t op_count = 0;
  startTracking = true;

  root_ptr->mapops->insert(root_ptr, UINT64_MAX, 0);
  msync(res->get_address(), res->get_size(), MS_SYNC);

  if (args.bulk) {
    perform_bulk_ops(res, args, root_ptr);
  } else {
    printf("Type 'h' for help\n$ ");

    nvsl::Clock clk;
    clk.tick();
    char buf[1024];
    while (fgets(buf, sizeof(buf), stdin)) {
      if (buf[0] == 0 || buf[0] == '\n') {
        std::cout << "$ ";
        continue;
      }

      // TX_BEGIN(res) {
      op_count++;

      switch (buf[0]) {
      case 'i':
        {
          if (strlen(buf) < 3) {
            print_help();
            break;
          }

          auto arg = std::string(buf + 1);
          root_ptr->mapops->insert(root_ptr, std::stoull(arg), 0);
          msync(res->get_address(), res->get_size(), MS_SYNC);
          break;
        }
      case 'p':
        {
          MapOps::foreach_cb cb = [](uint64_t k, uint64_t v, void *_) -> int {
            std::cout << k << " ";
            return 0;
          };

          root_ptr->mapops->foreach (root_ptr, cb);

          std::cout << std::endl;

          break;
        }
      case 'r':
        {
          if (strlen(buf) < 4) {
            print_help();
            break;
          }

          const auto arg = std::string(buf + 1);
          const auto arg_u64 = std::stoull(arg);
          root_ptr->mapops->remove(root_ptr, arg_u64);
          msync(res->get_address(), res->get_size(), MS_SYNC);

          break;
        }
      case 'h':
        print_help();
        break;
      default:
        std::cout << "Unknown command, press 'h' for help" << std::endl;
        break;
      }
      // }
      // TX_END;

      std::cout << "$ ";
    }
    clk.reconcile();
    clk.tock();

    std::cout << clk.summarize() << std::endl;
  }

  // res->close();

  return 0;
}
