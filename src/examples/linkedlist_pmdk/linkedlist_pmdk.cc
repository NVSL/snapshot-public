// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   linkedlist_pmdk.cc
 * @date   October 26, 2021
 * @brief  Brief description here
 */

#include <filesystem>
#include <libpmemobj.h>
#include <libpmemobj/base.h>
#include <libpmemobj/pool_base.h>
#include <libpmemobj/tx.h>

#include <chrono>
#include <cstring>
#include <exception>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "nvsl/clock.hh"

POBJ_LAYOUT_BEGIN(queue);
POBJ_LAYOUT_ROOT(queue, struct linkedlist_pmdk_t)
POBJ_LAYOUT_TOID(queue, struct node_pmdk_t)
POBJ_LAYOUT_END(queue)

using namespace std::chrono;
namespace fs = std::filesystem;

struct node_pmdk_t {
public:
  TOID(struct node_pmdk_t) next;
  uint64_t data;
};

struct linkedlist_pmdk_t {
public:
  TOID(struct node_pmdk_t) head;
  TOID(struct node_pmdk_t) tail;
};

struct args_t {
  std::string pool_path;
};

args_t parse_args(std::vector<std::string> args_v) {
  if (args_v.size() != 2) {
    std::cerr << "Usage: \n\t" << args_v[0] << " <pool>" << std::endl;
    exit(1);
  }

  args_t args = {};
  args.pool_path = args_v[1];

  return args;
}

void append_node(PMEMobjpool *pool, linkedlist_pmdk_t *ll_obj, uint64_t val) {
  TX_BEGIN(pool) {
    pmemobj_tx_add_range_direct(ll_obj, sizeof(linkedlist_pmdk_t));

    if (TOID_IS_NULL(ll_obj->head)) {
      TOID(struct node_pmdk_t) head = TX_NEW(struct node_pmdk_t);

      if (TOID_IS_NULL(head)) {
        throw std::runtime_error("TX_NEW failed");
      }

      ll_obj->head = head;
      ll_obj->tail = head;

    } else {
      TOID(struct node_pmdk_t) newTail = TX_NEW(struct node_pmdk_t);

      if (TOID_IS_NULL(newTail)) {
        throw std::runtime_error("res->malloc<>() failed");
      }

      TX_ADD(ll_obj->tail);

      D_RW(ll_obj->tail)->next = newTail;
      ll_obj->tail = newTail;
    }

    D_RW(ll_obj->tail)->data = val;
  }
  TX_ONABORT { std::cout << "Tx aborted" << std::endl; }
  TX_END;
}

void pop_head(PMEMobjpool *pop, linkedlist_pmdk_t *ll_obj) {
  TX_BEGIN(pop) {
    TX_ADD_DIRECT(ll_obj);
    auto cur_head = ll_obj->head;
    ll_obj->head = D_RO(cur_head)->next;
    TX_FREE(cur_head);
  }
  TX_ONABORT {
    std::cerr << "TX ABORT " << __FILE__ << ":" << __LINE__ << std::endl;
  }
  TX_END
}

void count_nodes(linkedlist_pmdk_t *ll_obj) {}

void print_nodes(linkedlist_pmdk_t *ll_obj) {
  node_pmdk_t *cur_node = D_RW(ll_obj->head);
  while (cur_node) {
    std::cout << cur_node->data << " ";
    cur_node = D_RW(cur_node->next);
  }

  std::cout << std::endl;

  std::cout << "pointers = " << std::endl;
  cur_node = D_RW(ll_obj->head);
  while (cur_node) {
    std::cout << " " << (void *)(cur_node) << ": " << cur_node->data << "\n";
    cur_node = D_RW(cur_node->next);
  }
  std::cout << std::endl;
}

void print_head(linkedlist_pmdk_t *ll_obj) {
  if (!TOID_IS_NULL(ll_obj->head)) {
    std::cerr << D_RO(ll_obj->head)->data << std::endl;
  }
}

size_t sum_nodes(linkedlist_pmdk_t *ll_obj) {
  node_pmdk_t *cur_node = D_RW(ll_obj->head);
  size_t result = 0;
  nvsl::Clock clk;

  clk.tick();
  while (cur_node != nullptr) {
    result += cur_node->data;
    cur_node = D_RW(cur_node->next);
  }
  clk.tock();
  clk.reconcile();
  std::cout << clk.summarize() << std::endl;

  return result;
}

void print_help() {
  std::cout << "Help: " << std::endl;
  std::cout << "\tA <int>  : "
            << "Do n append operations on the data structure" << std::endl;
  std::cout << "\tD <int>  : "
            << "Do n pop head operations on the data structure" << std::endl;
  std::cout << "\tf <int>  : "
            << "Print the head" << std::endl;
  std::cout << "\ti <int>  : "
            << "Appends an integer to the tail of the list" << std::endl;
  std::cout << "\tr        : "
            << "Removes the value at the tail of the list" << std::endl;
  std::cout << "\tp        : "
            << "Prints all the values in the list to stdout" << std::endl;
  std::cout << "\tc        : "
            << "Prints the length of the list to stdout" << std::endl;
}

void bulk_append(PMEMobjpool *pool, linkedlist_pmdk_t *ll_obj,
                 const uint64_t cnt) {
  size_t append_ops = 0, pop_ops = 0, print_ops = 0;

  nvsl::Clock clk;
  clk.tick();
  for (auto i = 0ULL; i < cnt; ++i) {
    // size_t rnd = rand() % 10;
    append_node(pool, ll_obj, i);

    // if (rnd < 2) {
    //   pop_head(pool, ll_obj);
    //   pop_ops++;
    // } else if (rnd < 4) {
    //   print_head(ll_obj);
    //   print_ops++;
    // }else {
    //   append_node(pool, ll_obj, i);
    //   append_ops++;
    // }
  }
  clk.tock();
  clk.reconcile();

  std::cout << clk.summarize() << std::endl;

  auto print_stat = [](size_t val, size_t tot, std::string name) {
    std::cout << name << " = " << val << " (% = " << (val * 100) / (double)tot
              << ")" << std::endl;
  };

  auto tot = pop_ops + append_ops + print_ops;
  print_stat(pop_ops, tot, "pop_ops");
  print_stat(print_ops, tot, "print_ops");
  print_stat(append_ops, tot, "append_ops");
}

void bulk_delete(PMEMobjpool *pool, linkedlist_pmdk_t *ll_obj,
                 const uint64_t cnt) {
  nvsl::Clock clk;
  clk.tick();
  for (auto i = 0ULL; i < cnt; ++i) {
    pop_head(pool, ll_obj);
  }
  clk.tock();
  clk.reconcile();

  std::cout << clk.summarize() << std::endl;
}

int main(int argc, char *argv[]) {
  args_t args = parse_args(std::vector<std::string>(argv, argv + argc));

  struct pmemobjpool *pool = nullptr;

  if (fs::is_regular_file(args.pool_path)) {
    std::cout << "Opening existing pool" << std::endl;
    pool = pmemobj_open(args.pool_path.c_str(), "linkedlist");
  } else {
    std::cout << "Created a new pool" << std::endl;
    pool = pmemobj_create(args.pool_path.c_str(), "linkedlist",
                          PMEMOBJ_MIN_POOL * 500, 0600);
  }

  TOID(struct linkedlist_pmdk_t)
  root = POBJ_ROOT(pool, struct linkedlist_pmdk_t);

  if (TOID_IS_NULL(root)) {
    std::cout << "Allocating root" << std::endl;

    TX_BEGIN(pool) {
      TX_ADD(root);

      root = TX_NEW(struct linkedlist_pmdk_t);
    }
    TX_END;

    if (TOID_IS_NULL(root)) {
      throw std::runtime_error("Allocating root failed");
    }
  } else {
    std::cout << "Got previous root" << std::endl;
  }

  size_t op_count = 0;

  printf("Type 'h' for help\n$ ");

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
      append_node(pool, D_RW(root), std::stoull(std::string(buf + 1)));
      break;
    case 'c':
      count_nodes(D_RW(root));
      break;
    case 'p':
      print_nodes(D_RW(root));
      break;
    case 'f':
      print_head(D_RW(root));
      break;
    case 'd':
      pop_head(pool, D_RW(root));
      break;
    case 's':
      std::cout << sum_nodes(D_RW(root)) << std::endl;
      break;
    case 'A':
      if (strlen(buf) < 4) {
        print_help();
        break;
      }
      bulk_append(pool, D_RW(root), std::stoull(std::string(buf + 1)));
      break;
    case 'D':
      if (strlen(buf) < 4) {
        print_help();
        break;
      }
      bulk_delete(pool, D_RW(root), std::stoull(std::string(buf + 1)));
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

  return 0;
}
