// #include "libpuddles/libpuddles.h"

#include <boost/interprocess/interprocess_fwd.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>
#include <chrono>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "../map/map.hh"
#include "btree.hh"
// #include "libpuddles/libpuddles.h"
// #include "puddles/constants.hh"

void btree_node_t::init() {
  this->count = 0;
}

void btree_node_t::set_item(size_t pos, const btree_node_item_t &item) {
  this->items[pos] = item;
  this->count++;
}

void btree_node_t::clear(size_t pos) {
  this->items[pos].key = 0UL;
  this->items[pos].val = 0UL;
}

void btree_t::init() {
  this->root = nullptr;
}

void btree_t::clear_node(btree_node_t *node) {
  if (node == nullptr) return;
  for (size_t i = 0; i < node->count; ++i) {
    clear_node(node->children[i]);
  }

  tls_res->deallocate(node);
}

int btree_t::clear(bip::managed_mapped_file *res) {
  int ret = 0;
  // TX_BEGIN(res) {
    clear_node(root);

    // TX_ADD(&root);

    root = nullptr;
  // }
  // TX_END;

  return ret;
}

void btree_t::insert_node_empty_tree(btree_node_item_t item) {
  // TX_ADD(&root);

  if (root == nullptr) root = (btree_node_t*)tls_res->allocate(sizeof(btree_node_t));
  root->init();
  root->set_item(0, item);
}

void btree_t::insert_node(btree_node_t *node, size_t pos,
                          const btree_node_item_t &item,
                          btree_node_t *left /* = nullptr */,
                          btree_node_t *right /* = nullptr */) {
  // TX_ADD(node);
  if (node->items[pos].key != 0) { /* move all existing data */
    memmove(&node->items[pos + 1], &node->items[pos],
            sizeof(btree_node_item_t) * (BTREE_ORDER - 2 - pos));

    memmove(&node->children[pos + 1], &node->children[pos],
            sizeof(btree_node_t *) * (BTREE_ORDER - 1 - pos));
  }

  node->children[pos] = left;
  node->children[pos + 1] = right;

  node->set_item(pos, item);
}

btree_node_t *btree_t::split_node(btree_node_t *node, btree_node_item_t *item) {
  auto right = (btree_node_t*)tls_res->allocate(sizeof(btree_node_t));
  right->init();

  int c = (BTREE_ORDER / 2);
  *item = node->items[c - 1]; /* select median item */
  // TX_ADD(node);
  node->clear(c - 1);

  /* move everything right side of median to the new node */
  for (size_t i = c; i < BTREE_ORDER; ++i) {
    if (i != BTREE_ORDER - 1) {
      right->items[right->count++] = node->items[i];
      node->clear(i);
    }
    right->children[i - c] = node->children[i];
    node->children[i] = nullptr;
  }
  node->count = c - 1;

  return right;
}

btree_node_t *btree_t::find_dest_node(btree_node_t *node, btree_node_t *parent,
                                      uint64_t key, size_t *pos) {
  if (node->count == BTREE_ORDER - 1) { /* node is full, perform a split */
    btree_node_item_t m;
    btree_node_t *right = split_node(node, &m);

    if (parent != nullptr) {
      insert_node(parent, *pos, m, node, right);
      if (key > m.key) /* select node to continue search */
        node = right;
    } else { /* replacing root node, the tree grows in height */
      btree_node_t *up = (btree_node_t*)tls_res->allocate(sizeof(btree_node_t));
      up->init();
      up->count = 1;
      up->items[0] = m;
      up->children[0] = node;
      up->children[1] = right;

      // TX_ADD(&root);
      root = up;
      node = up;
    }
  }

  size_t i;
  for (i = 0; i < BTREE_ORDER - 1; ++i) {
    *pos = i;

    /*
     * The key either fits somewhere in the middle or at the
     * right edge of the node.
     */
    if (node->count == i || node->items[i].key > key) {
      return node->children[i] == nullptr
                 ? node
                 : find_dest_node(node->children[i], node, key, pos);
    }
  }

  /*
   * The key is bigger than the last node element, go one level deeper
   * in the rightmost child.
   */
  return find_dest_node(node->children[i], node, key, pos);
}

void btree_t::insert_item(btree_node_t *node, size_t pos,
                          btree_node_item_t item) {
  // TX_ADD(node);
  if (node->items[pos].key != 0) {
    memmove(&node->items[pos + 1], &node->items[pos],
            sizeof(btree_node_item_t) * ((BTREE_ORDER - 2 - pos)));
  }
  node->set_item(pos, item);
}

bool btree_t::is_empty() {
  return root == nullptr || root->count == 0;
}

int btree_t::insert(bip::managed_mapped_file *res, uint64_t key, uint64_t value) {
  btree_node_item_t item = {key, value};
  // TX_BEGIN(res) {
    if (is_empty()) {
      insert_node_empty_tree(item);
    } else {
      size_t pos; /* position at the dest node to insert */
      btree_node_t *parent = nullptr;
      btree_node_t *dest = find_dest_node(root, parent, key, &pos);

      insert_item(dest, pos, item);
    }
  // }
  // TX_END;

  return 0;
}

void btree_t::rotate_right(btree_node_t *rsb, btree_node_t *node,
                           btree_node_t *parent, size_t pos) {
  /* move the separator from parent to the deficient node */
  btree_node_item_t sep = parent->items[pos];
  insert_item(node, node->count, sep);

  /* the first element of the right sibling is the new separator */
  // TX_ADD(&parent->items[pos]); // !! TX_ADD_FIELD
  parent->items[pos] = rsb->items[0];

  /* the nodes are not necessarily leafs, so copy also the slot */
  // TX_ADD(&node->children[node->count]); // !! TX_ADD_FIELD
  node->children[node->count] = rsb->children[0];

  // TX_ADD(rsb);
  rsb->count -= 1; /* it loses one element, but still > min */

  /* move all existing elements back by one array slot */
  memmove(rsb->items, rsb->items + 1, sizeof(btree_node_item_t) * (rsb->count));
  memmove(rsb->children, rsb->children + 1,
          sizeof(btree_node_t *) * (rsb->count + 1));
}

void btree_t::rotate_left(btree_node_t *lsb, btree_node_t *node,
                          btree_node_t *parent, size_t pos) {
  /* move the separator from parent to the deficient node */
  btree_node_item_t sep = parent->items[pos - 1];
  insert_item(node, 0, sep);

  /* the last element of the left sibling is the new separator */
  // TX_ADD(&parent->items[pos - 1]); // !! TX_ADD_FIELD
  parent->items[pos - 1] = lsb->items[lsb->count - 1];

  /* rotate the node children */
  memmove(node->children + 1, node->children,
          sizeof(btree_node_t *) * (node->count));

  /* the nodes are not necessarily leafs, so copy also the slot */
  node->children[0] = lsb->children[lsb->count];

  // TX_ADD_FIELD(lsb, n);
  // TX_ADD(&lsb->count); // !! TX_ADD_FIELD
  lsb->count -= 1;     /* it loses one element, but still > min */
}

void btree_t::merge(btree_node_t *rn, btree_node_t *node, btree_node_t *parent,
                    size_t pos) {
  btree_node_item_t sep = parent->items[pos];

  // TX_ADD(node);
  /* add separator to the deficient node */
  node->items[node->count++] = sep;

  /* copy right sibling data to node */
  memcpy(&node->items[node->count], rn->items,
         sizeof(btree_node_item_t) * rn->count);
  memcpy(&node->children[node->count], rn->children,
         sizeof(btree_node_t *) * (rn->count + 1));

  node->count += rn->count;

  // !! TX_FREE
  tls_res->deallocate(rn); /* right node is now empty */

  // TX_ADD(parent);
  parent->count -= 1;

  /* move everything to the right of the separator by one array slot */
  memmove(parent->items + pos, parent->items + pos + 1,
          sizeof(btree_node_item_t) * (parent->count - pos));

  memmove(parent->children + pos + 1, parent->children + pos + 2,
          sizeof(btree_node_t *) * (parent->count - pos + 1));

  /* if the parent is empty then the tree shrinks in height */
  if (parent->count == 0 && parent == root) {
    // TX_ADD(this);
    tls_res->deallocate(root); // !! TX_FREE(D_RO(map)->root);
    root = node;
  }
}

void btree_t::rebalance(btree_node_t *node, btree_node_t *parent, size_t pos) {
  btree_node_t *rsb =
      pos >= parent->count ? nullptr : parent->children[pos + 1];
  btree_node_t *lsb = pos == 0 ? nullptr : parent->children[pos - 1];

  if (rsb != nullptr && rsb->count > BTREE_MIN)
    rotate_right(rsb, node, parent, pos);
  else if (lsb != nullptr && lsb->count > BTREE_MIN)
    rotate_left(lsb, node, parent, pos);
  else if (rsb == nullptr) /* always merge with rightmost node */
    merge(node, lsb, parent, pos - 1);
  else
    merge(rsb, node, parent, pos);
}

btree_node_t *btree_t::get_leftmost_leaf(btree_node_t *node,
                                         btree_node_t **parent) {
  if (node->children[0] == nullptr) return node;

  *parent = node;

  return get_leftmost_leaf(node->children[0], parent);
}

void btree_t::remove_from_node(btree_node_t *node, btree_node_t *parent,
                               size_t pos) {
  if (node->children[0] == nullptr) { /* leaf */
    // TX_ADD(node);
    if (node->count == 1 || pos == BTREE_ORDER - 2) {
      node->clear(pos);
    } else if (node->count != 1) {
      memmove(&node->items[pos], &node->items[pos + 1],
              sizeof(btree_node_item_t) * (node->count - pos));
    }

    node->count -= 1;
    return;
  }

  /* can't delete from non-leaf nodes, remove successor */
  btree_node_t *rchild = node->children[pos + 1];
  btree_node_t *lp = node;
  btree_node_t *lm = get_leftmost_leaf(rchild, &lp);

  // TX_ADD_FIELD(node, items[p]);
  // TX_ADD(&node->items[pos]); // !! TX_ADD_FIELD
  node->items[pos] = lm->items[0];

  remove_from_node(lm, lp, 0);

  if (lm->count < BTREE_MIN) /* right child can be deficient now */
    rebalance(lm, lp, lp == node ? pos + 1 : 0);
}

uint64_t btree_t::remove_item(btree_node_t *node, btree_node_t *parent,
                              uint64_t key, size_t pos) {
  uint64_t ret = 0; // TODO: we need a not-found val std::tuple<bool, uint64_t>
  for (size_t i = 0; i <= node->count; ++i) {
    if (NODE_CONTAINS_ITEM(node, i, key)) {
      ret = node->items[i].val;
      remove_from_node(node, parent, i);
      break;
    } else if (NODE_CHILD_CAN_CONTAIN_ITEM(node, i, key)) {
      ret = remove_item(node->children[i], node, key, i);
      break;
    }
  }

  /* check for deficient nodes walking up */
  if (parent != nullptr && node->count < BTREE_MIN)
    rebalance(node, parent, pos);

  return ret;
}

uint64_t btree_t::remove(bip::managed_mapped_file *res, uint64_t key) {
  uint64_t ret = 0; // TODO: we need a not-found val
  // TX_BEGIN(res) {
    ret = remove_item(root, nullptr, key, 0);
  // }
  // TX_END;

  return ret;
}

uint64_t btree_t::get_in_node(btree_node_t *node, uint64_t key) {
  for (size_t i = 0; i <= node->count; ++i) {
    if (NODE_CONTAINS_ITEM(node, i, key))
      return node->items[i].val;
    else if (NODE_CHILD_CAN_CONTAIN_ITEM(node, i, key))
      return get_in_node(node->children[i], key);
  }

  return 0; // TODO: we need a not-found val
}

uint64_t btree_t::get(uint64_t key) {
  if (root == nullptr) return 0; // TODO: we need a not-found val
  return get_in_node(root, key);
}

int btree_t::lookup_in_node(btree_node_t *node, uint64_t key) {
  for (size_t i = 0; i <= node->count; ++i) {
    if (NODE_CONTAINS_ITEM(node, i, key))
      return 1;
    else if (NODE_CHILD_CAN_CONTAIN_ITEM(node, i, key))
      return lookup_in_node(node->children[i], key);
  }

  return 0;
}

int btree_t::btree_map_lookup(uint64_t key) {
  if (root == nullptr) return 0;
  return lookup_in_node(root, key);
}

int btree_t::foreach_node(const btree_node_t *node,
                          int (*cb)(uint64_t key, uint64_t, void *arg),
                          void *arg) {
  if (node == nullptr) return 0;

  for (size_t i = 0; i <= node->count; ++i) {
    if (foreach_node(node->children[i], cb, arg) != 0) return 1;

    if (i != node->count && node->items[i].key != 0) {
      if (cb(node->items[i].key, node->items[i].val, arg) != 0) return 1;
    }
  }
  return 0;
}

int btree_t::foreach (int (*cb)(uint64_t key, uint64_t value, void *arg),
                      void *arg) {
  return foreach_node(root, cb, arg);
}
