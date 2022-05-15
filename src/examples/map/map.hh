// -*- mode: c++-mode; c-basic-offset: 2; -*-

#pragma once

#include "boost/interprocess/mem_algo/rbtree_best_fit.hpp"
#include "boost/interprocess/offset_ptr.hpp"
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/indexes/iset_index.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/mem_algo/simple_seq_fit.hpp>
#include <boost/interprocess/sync/mutex_family.hpp>
#include <functional>
#include <string>

namespace bip = boost::interprocess;

typedef bip::rbtree_best_fit<bip::null_mutex_family, void *> null_mutex_ator;
typedef bip::basic_managed_mapped_file<char, null_mutex_ator, bip::iset_index>
    mmf;

struct MapOps;

struct MapRoot {
public:
  void *ptr;
  MapOps *mapops;

  template <typename T>
  T *operator()(const MapRoot &obj) {
    return (T *)(obj.ptr);
  }
};

struct MapOps {
public:
  bip::managed_mapped_file *res;

  /* Function signature for a callback */
  typedef int (*foreach_cb)(uint64_t, uint64_t, void *);

  std::function<void(void *, uint64_t, uint64_t)> insert;
  std::function<void(void *, uint64_t)> remove;
  std::function<uint64_t(void *, uint64_t)> get;
  std::function<void(void *, foreach_cb)> foreach;
  std::function<bool(void *, uint64_t)> lookup;
};

class Args {
public:
  std::string puddle_path;

  Args(int argc, char *argv[]);
};

enum map_backend_t
{
  BTREE = 0,
  MAX = 1
};

void print_help();
