// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   list.hh
 * @date   May 10, 2021
 * @brief  Brief description here
 */

#pragma once

#include <boost/interprocess/managed_mapped_file.hpp>
#include <stdint.h>

namespace bip = boost::interprocess;

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
