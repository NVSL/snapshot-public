// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   ator.cc
 * @date   avril 28, 2022
 * @brief  Brief description here
 */

#include <boost/interprocess/indexes/iset_index.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/mem_algo/rbtree_best_fit.hpp>
#include <boost/interprocess/sync/mutex_family.hpp>
#include <boost/interprocess/mem_algo/simple_seq_fit.hpp>
#include <stdint.h>

#include "boost/interprocess/creation_tags.hpp"
#include "libpmdkemul/ator.h"
#include "libstoreinst.hh"
#include "nvsl/common.hh"

namespace bip = boost::interprocess;

typedef bip::simple_seq_fit<bip::null_mutex_family> null_mutex_ator;
typedef bip::basic_managed_mapped_file<char, null_mutex_ator, bip::iset_index>
mmf;

void *mmf_create(const char *path, size_t bytes) {
  auto res = new mmf(bip::open_or_create, path, bytes);
  return (void*)res;
}

void *mmf_alloc(void *mmf_obj, size_t bytes) {
  assert(mmf_obj != nullptr);

  auto res = nvsl::RCast<mmf *>(mmf_obj);
  
  return res->allocate(bytes);
}

void mmf_free(void *mmf_obj, void *ptr) {
  assert(mmf_obj != nullptr);

  //auto res = nvsl::RCast<mmf *>(mmf_obj);
  
  //  res->deallocate(ptr);
}

void *mmf_get_root(void *mmf_obj, size_t bytes) {
  assert(mmf_obj != nullptr);

  auto res = nvsl::RCast<mmf *>(mmf_obj);
  
  return res->find<void *>("root").first;
}

void *mmf_alloc_root(void *mmf_obj, size_t bytes) {
  assert(mmf_obj != nullptr);

  auto res = nvsl::RCast<mmf *>(mmf_obj);
  
  auto ctor = res->construct<void *>("root");
  auto ptr = ctor();

  return ptr;
}

void *mmf_get_or_alloc_root(void *mmf_obj, size_t bytes) {
  void *result = NULL;

  assert(mmf_obj != nullptr);

  if (mmf_get_root(mmf_obj, bytes) == NULL) {
    result = mmf_alloc_root(mmf_obj, bytes);
  }

  result = mmf_get_root(mmf_obj, bytes);

  
  return result;
}

void mmf_snapshot(void *mmf_obj) {
  assert(mmf_obj != nullptr);

  auto res = nvsl::RCast<mmf *>(mmf_obj);

  const auto start_addr = res->get_address();
  const auto len = res->get_size();
  
  snapshot(start_addr, len, MS_SYNC);
}
