// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-

#include "../list.hh"
#include "libstoreinst.hh"
#include "nvsl/clock.hh"
#include "nvsl/common.hh"
#include "nvsl/common.impl.hh"
#include "nvsl/stats.hh"

#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/indexes/iset_index.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>
#include <boost/interprocess/sync/mutex_family.hpp>
#include <chrono>
#include <cstring>
#include <exception>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace nvsl;
using namespace std::chrono;

constexpr size_t MIN_POOL_SZ = 1024 * 1024;
extern bool startTracking;

struct args_t {
  std::string puddle_path;
};

void print_help() {
  // clang-format off
  std::cout << "Help: " << std::endl;
  std::cout << "\ti <n:int>                 : "
            << "Appends n to the tail of the list"
            << std::endl;
  std::cout << "\tA <n:int>                 : "
            << "Bulk insert n values"
            << std::endl;
  std::cout << "\tR <n:int>                 : "
            << "Bulk delete n values from the tail"
            << std::endl;
  std::cout << "\tf <n:int>                 : "
            << "Append n to the tail and exit before completing the tx"
            << std::endl;
  std::cout << "\tg <path:str> <TAR_GZ|DIR> : "
            << "Package the reservoir to the path with package type <TAR_GZ|DIR>"
            << std::endl;
  std::cout << "\tr                         : "
            << "Removes the value at the tail of the list"
            << std::endl;
  std::cout << "\tp                         : "
            << "Prints all the values in the list to stdout"
            << std::endl;
  std::cout << "\tc                         : "
            << "Prints the length of the list to stdout"
            << std::endl;
  // clang-format on
}

args_t parse_args(std::vector<std::string> args_v) {
  if (args_v.size() != 2) {
    std::cerr << "Usage: \n\t" << args_v[0] << " <puddle>" << std::endl;
    exit(1);
  }

  args_t args = {};
  args.puddle_path = args_v[1];

  return args;
}


void append_node(mmf *res, linkedlist_t *ll_obj,
                 uint64_t val, bool exit_mid_tx = false) {
  // TX_BEGIN(res) {
    // TX_ADD(ll_obj);

    if (ll_obj->head == NULL) {
      node_t *head = (node_t*)res->allocate(sizeof(node_t));

      std::memset(head, 0, sizeof(node_t));

      if (head == NULL) {
        throw std::runtime_error("res->allocate<>() failed");
      }

      ll_obj->head = head;
      ll_obj->tail = head;

    } else {
      node_t *newTail = (node_t*)res->allocate(sizeof(node_t));

      if (newTail == nullptr) {
        throw std::runtime_error("res->allocate<>() failed");
      }

      std::memset(newTail, 0, sizeof(node_t));

      // TX_ADD(ll_obj->tail);

      ll_obj->tail->next = newTail;
      ll_obj->tail = newTail;

      // std::cerr << "Updated tail " << (void*)&ll_obj->tail << std::endl;
    }

    ll_obj->tail->data = val;

    if (exit_mid_tx) {
      std::cout << "*** EXITING MID TX ***" << std::endl;
      exit(0);
    }
    msync(res->get_address(), res->get_size(), MS_SYNC);

  // }
  // TX_END;
}

void count_nodes(linkedlist_t *ll_obj) {
  size_t count = 0;

  node_t *cur_node = ll_obj->head;
  std::cout << "cur_node = " << (void*)(cur_node) << std::endl;
  while (cur_node) {
    cur_node = cur_node->next;
    count++;
  }

  std::cout << "Count: " << count << std::endl;
}

void print_nodes(linkedlist_t *ll_obj) {
  node_t *cur_node = ll_obj->head;
  while (cur_node) {
    std::cout << cur_node->data << " ";
    cur_node = cur_node->next;
  }

  std::cout << std::endl;

  std::cout << "pointers = " << std::endl;
  cur_node = ll_obj->head;
  while (cur_node) {
    std::cout << " " << (void*)(cur_node) << ": " << cur_node->data << "\n";
    cur_node = cur_node->next;
  }
  std::cout << std::endl;
}

void package_reservoir(mmf *res, const std::string &path,
                       const std::string &pkg_type_str) {
  NVSL_ASSERT(false, "Unimplemented");
}

void pop_head(mmf *res, linkedlist_t *ll_obj) {
  // TX_BEGIN(res) {
    if (ll_obj->head == nullptr) {
      // std::cout << "list empty" << std::endl;
    } else {
      // TX_ADD(ll_obj);
      node_t *oldNode;
      if (ll_obj->head == ll_obj->tail) {
        oldNode = ll_obj->head;
        ll_obj->head = nullptr;
        ll_obj->tail = nullptr;
      } else {
        oldNode = ll_obj->head;
        node_t *newHead = ll_obj->head->next;
        ll_obj->head = newHead;
      }
      res->deallocate(oldNode);
      // TX_ADD_ON_COMPLETE(([=]() { res->free(oldNode); }));
    }
  // }
  // TX_END;
    msync(res->get_address(), res->get_size(), MS_SYNC);
}

void bulk_append_nodes(mmf *res, linkedlist_t *ll_obj,
                       const uint64_t cnt) {
  Clock clk;
  clk.tick();
  for (auto i = 0ULL; i < cnt; ++i) {
    append_node(res, ll_obj, i);
  }
  clk.tock();
  clk.reconcile();

  std::cout << clk.summarize(cnt) << std::endl;
}

void bulk_append_delete_nodes(mmf *res, linkedlist_t *ll_obj,
                              const uint64_t cnt, const double ratio = 1) {
  Clock clk;
  clk.tick();
  const int thresh = RAND_MAX / (1 + ratio) * ratio;
  for (auto i = 0ULL; i < cnt; ++i) {
    if (rand() > thresh) {
      append_node(res, ll_obj, i);
    } else {
      pop_head(res, ll_obj);
    }
  }
  clk.tock();
  clk.reconcile();

  std::cout << clk.summarize(cnt) << std::endl;
}

void bulk_delete_nodes(mmf *res, linkedlist_t *ll_obj,
                       const uint64_t cnt) {
  Clock clk;
  clk.tick();
  for (auto i = 0ULL; i < cnt; ++i) {
    pop_head(res, ll_obj);
  }
  clk.tock();
  clk.reconcile();

  std::cout << clk.summarize(cnt) << std::endl;
}

size_t sum_nodes(const linkedlist_t *ll_obj) {
  node_t *cur_node = ll_obj->head;
  size_t result = 0;
  Clock clk;

  clk.tick();
  while (cur_node != nullptr) {
    result += cur_node->data;
    cur_node = cur_node->next;
  }
  clk.tock();
  clk.reconcile();

  std::cout << clk.summarize() << std::endl;

  return result;
}

int main(int argc, char *argv[]) {
  // register_filt_fun(typeid(node_t), node_t::filter);
  // register_filt_fun(typeid(linkedlist_t), linkedlist_t::filter);

  args_t args = parse_args(std::vector<std::string>(argv, argv + argc));

  auto res = new mmf(bip::open_or_create, args.puddle_path.c_str(),
                     MIN_POOL_SZ*1000);
  // reservoir_t *res =
      // new reservoir_t(libpuddles::MIN_POOL_SZ * 10, args.puddle_path.c_str());


  auto root_ptr = res->find<linkedlist_t>("root").first;

  if (root_ptr == NULL) {
    std::cout << "Allocating root" << std::endl;

    // TX_BEGIN(res) { 
    root_ptr = res->construct<linkedlist_t>("root")();
    std::cerr << "Root allocated at " << (void*)root_ptr << std::endl;
    // }
    // TX_END;

    if (root_ptr == NULL) {
      throw std::runtime_error("Allocating root failed");
    }

    memset(root_ptr, 0, sizeof(*root_ptr));
  } else {
    std::cout << "Got previous root" << (void*)root_ptr << std::endl;
  }

  // printf("Root uuid: %s\n", res->get_root_uuid().to_string().c_str());

  size_t op_count = 0;

  printf("Type 'h' for help\n$ ");

  startTracking = true;
  append_node(res, root_ptr, 1);
  pop_head(res, root_ptr);
  msync(root_ptr, 4096, MS_SYNC);

  char buf[1024];
  while (fgets(buf, sizeof(buf), stdin)) {
    if (buf[0] == 0 || buf[0] == '\n') {
      std::cout << "$ ";
      continue;
    }

    op_count++;

    switch (buf[0]) {
    case 'i':
      if (strlen(buf) < 4) {
        print_help();
        break;
      }
      append_node(res, root_ptr, std::stoull(std::string(buf + 1)));
      break;
    case 'c':
      count_nodes(root_ptr);
      break;
    case 's':
      std::cout << sum_nodes(root_ptr) << std::endl;
      break;
    case 'g':
      {
        const auto toks = split(std::string(buf, strlen(buf) - 1), " ");
        if (toks.size() != 3) {
          print_help();
          break;
        }
        std::cout << "packaging..." << std::endl;
        package_reservoir(res, toks[1], toks[2]);
      }
      break;
    case 'p':
      print_nodes(root_ptr);
      break;
    case 'f':
      if (strlen(buf) < 4) {
        print_help();
        break;
      }
      append_node(res, root_ptr, std::stoull(std::string(buf + 1)), true);
      break;
    case 'A':
      {
        if (strlen(buf) < 4) {
          print_help();
          break;
        }
        bulk_append_nodes(res, root_ptr, std::stoull(std::string(buf + 1)));
        break;
      }
    case 'r':
      pop_head(res, root_ptr);
      break;
    case 'R':
      if (strlen(buf) < 4) {
        print_help();
        break;
      }
      bulk_delete_nodes(res, root_ptr, std::stoull(std::string(buf + 1)));
      break;
    case 'h':
      print_help();
      break;
    default:
      std::cout << "Unknown command, press 'h' for help" << std::endl;
      break;
    }

    std::cout << "$ ";
  }

  // res->close();

  return 0;
}
