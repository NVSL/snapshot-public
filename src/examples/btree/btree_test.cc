// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   btree_test.cc
 * @date   octobre 21, 2021
 * @brief  Brief description here
 */

#include "btree.hh"

#include <iostream>
#include <ranges>

int main() {
  // auto res = new reservoir_t(MIN_POOL_SZ * 10, "");

  // auto root = res->get_root<btree_t>();

  // if (root == nullptr) {
  //   root = res->malloc<btree_t>();

  //   // std::memset(root, 0, sizeof(*root));
  //   root->init();
  // }

  // std::vector<uint64_t> ins_vec = {1, 2, 3, 4, 5, 6, 100, 1000, 0};
  // std::vector<uint64_t> del_vec;

  // uint64_t ins_vec_iter = 0;
  // auto even = [ins_vec_iter](uint64_t val) mutable {
  //   return ins_vec_iter++ % 2 == 0;
  // };

  // for (auto i : ins_vec | std::views::filter(even))
  //   del_vec.push_back(i);

  // std::cout << "Insert vector = ";
  // for (auto entry : ins_vec)
  //   std::cout << entry << " ";
  // std::cout << std::endl;

  // std::cout << "Delete vector = ";
  // for (auto entry : del_vec)
  //   std::cout << entry << " ";
  // std::cout << std::endl;

  // std::cout << "Inserting..." << std::endl;

  // for (auto entry : ins_vec) {
  //   root->insert(res, entry, 0);
  // }

  // std::cout << "Deleting..." << std::endl;

  // for (auto entry : ins_vec) {
  //   root->remove(res, entry);
  // }
}
