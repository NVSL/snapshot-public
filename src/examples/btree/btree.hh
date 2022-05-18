// -*- mode: c++-mode; c-basic-offset: 2; -*-

#pragma once

/**
 * @file   btree.hh
 * @date   December 22, 2020
 * @brief  Brief description here
 */

#include <boost/interprocess/managed_mapped_file.hpp>
#include <cstddef>
#include <cstdint>

namespace bip = boost::interprocess;

typedef bip::rbtree_best_fit<bip::null_mutex_family, void *> null_mutex_ator;
typedef bip::basic_managed_mapped_file<char, null_mutex_ator, bip::iset_index>
    mmf;

extern mmf *tls_res;

const size_t BTREE_ORDER = 8; /* can't be odd */
const size_t BTREE_MIN =
    ((BTREE_ORDER / 2) - 1); /* min number of keys per node */

struct btree_node_item_t {
  uint64_t key;
  uint64_t val;
};

struct btree_node_t {
  uint64_t count; /* Number of occupied slots */
  btree_node_item_t items[BTREE_ORDER - 1];
  btree_node_t *children[BTREE_ORDER];

  void init();
  void set_item(size_t pos, const btree_node_item_t &item);
  void clear(size_t pos);
};

struct btree_t {
  btree_node_t *root;

  void init();

  void clear_node(btree_node_t *node);

  int clear(mmf *res);

  void insert_node_empty_tree(btree_node_item_t item);

  void insert_node(btree_node_t *node, size_t pos,
                   const btree_node_item_t &item, btree_node_t *left = nullptr,
                   btree_node_t *right = nullptr);

  btree_node_t *split_node(btree_node_t *node, btree_node_item_t *item);

  btree_node_t *find_dest_node(btree_node_t *node, btree_node_t *parent,
                               uint64_t key, size_t *pos);

  void insert_item(btree_node_t *node, size_t pos, btree_node_item_t item);

  bool is_empty();

  int insert(mmf *res, uint64_t key, uint64_t value);

  void rotate_right(btree_node_t *rsb, btree_node_t *node, btree_node_t *parent,
                    size_t pos);

  void rotate_left(btree_node_t *lsb, btree_node_t *node, btree_node_t *parent,
                   size_t pos);

  void merge(btree_node_t *rn, btree_node_t *node, btree_node_t *parent,
             size_t pos);

  void rebalance(btree_node_t *node, btree_node_t *parent, size_t pos);

  void remove_from_node(btree_node_t *node, btree_node_t *parent, size_t pos);

  btree_node_t *get_leftmost_leaf(btree_node_t *node, btree_node_t **parent);

  uint64_t remove_item(btree_node_t *node, btree_node_t *parent, uint64_t key,
                       size_t pos);

  uint64_t remove(mmf *res, uint64_t key);

#define NODE_CONTAINS_ITEM(_n, _i, _k) \
  ((_i) != (_n)->count && (_n)->items[_i].key == (_k))

#define NODE_CHILD_CAN_CONTAIN_ITEM(_n, _i, _k)          \
  ((_i) == (_n)->count || (_n)->items[_i].key > (_k)) && \
      ((_n)->children[_i] != nullptr)

  uint64_t get_in_node(btree_node_t *node, uint64_t key);

  uint64_t get(uint64_t key);

  int lookup_in_node(btree_node_t *node, uint64_t key);

  int btree_map_lookup(uint64_t key);

  int foreach_node(const btree_node_t *node,
                   int (*cb)(uint64_t key, uint64_t, void *arg), void *arg);

  int foreach (int (*cb)(uint64_t key, uint64_t value, void *arg), void *arg);
};
