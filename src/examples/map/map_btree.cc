// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   map_btree.cc
 * @date   octobre 20, 2021
 * @brief  Brief description here
 */

#include "map_btree.hh"
#include "../btree/btree.hh"
// #include "libpuddles/libpuddles.h"
#include <memory>

void insert(mmf *res, void *tree, uint64_t key, uint64_t val) {
  auto tree_btree = (btree_t *)(tree);

  tree_btree->insert(res, key, val);
}

void remove(mmf *res, void *tree, uint64_t key) {
  auto tree_btree = (btree_t *)(tree);

  tree_btree->remove(res, key);
}

uint64_t get(mmf *res, void *tree, uint64_t key) {
  auto tree_btree = (btree_t *)(tree);

  return tree_btree->get(key);
}

bool lookup(mmf *res, void *tree, uint64_t key) {
  auto tree_btree = (btree_t *)tree;

  return tree_btree->btree_map_lookup(key);
}

void foreach (mmf *res, void *tree, MapOps::foreach_cb cb) {
  auto tree_btree = (btree_t *)tree;

  tree_btree->foreach (cb, nullptr);
}

MapOps *map_btree(mmf *res) {
  auto result = new MapOps;

  result->insert = [res](void *tree, uint64_t key, uint64_t val) mutable {
    insert(res, tree, key, val);
  };

  result->remove = [res](void *tree, uint64_t key) mutable {
    remove(res, tree, key);
  };

  result->get = [&](void *tree, uint64_t key) { return get(res, tree, key); };

  result->lookup = [&](void *tree, uint64_t key) {
    return lookup(res, tree, key);
  };

  result->foreach = [&](void *tree, MapOps::foreach_cb cb) {
    return foreach (res, tree, cb);
  };

  return result;
}
