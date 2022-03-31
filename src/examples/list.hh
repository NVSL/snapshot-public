// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   list.hh
 * @date   May 10, 2021
 * @brief  Brief description here
 */

#pragma once

#include <boost/interprocess/indexes/iset_index.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/mem_algo/rbtree_best_fit.hpp>
#include <boost/interprocess/sync/mutex_family.hpp>
#include <boost/interprocess/mem_algo/simple_seq_fit.hpp>
#include <stdint.h>

namespace bip = boost::interprocess;

typedef bip::rbtree_best_fit<bip::null_mutex_family> null_mutex_ator;
typedef bip::basic_managed_mapped_file<char, null_mutex_ator, bip::iset_index>
mmf;

class node_t {
public:
  node_t *next;
  uint64_t data;
};

class linkedlist_t {
public:
  node_t *head;
  node_t *tail;
};
