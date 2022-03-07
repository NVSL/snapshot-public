// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019, Intel Corporation */

/*
 * simplekv.hpp -- implementation of simple kv which uses vector to hold
 * values, string as a key and array to hold buckets
 */
#include <cstddef>
#include <iostream>
#include <string>
#include <string>
#include <sys/mman.h>
#include <vector>

#include "libstoreinst.h"

extern "C" {
}

/**
 * Value - type of the value stored in hashmap
 * N - number of buckets in hashmap
 */
template <typename Value, std::size_t N>
class simple_kv {
private:
  using key_type = std::string;
  using bucket_elem_type = std::pair<key_type, std::size_t>;
  using bucket_type = std::vector<bucket_elem_type>;
  using value_vector = std::vector<Value>;

  bucket_type buckets[N];
  value_vector values;

  void *region;
  size_t region_sz_bytes;

public:
  simple_kv() = default;

  void init(void *region, size_t region_sz_bytes) {
    this->region = region;
    this->region_sz_bytes = region_sz_bytes;

    for (size_t i = 0; i < N; ++i) {
      new (&buckets[i]) bucket_type;
    }
    new (&values) value_vector;
  }

  const Value &get(const std::string &key) const {
    auto index = std::hash<std::string>{}(key) % N;

    size_t bucket_size = buckets[index].size();
    auto &tmp_bucket = buckets[index];
    for (size_t i = 0; i < bucket_size; ++i) {
      auto &key_pair = tmp_bucket[i];

      if (key_pair.first == key) return values[key_pair.second];
    }

    throw std::out_of_range("no entry in simplekv");
  }

  void put(const std::string &key, const Value &val) {
    auto index = std::hash<std::string>{}(key) % N;

    /* search for element with specified key - if found
     * transactionally update its value */
    size_t bucket_size = buckets[index].size();
    // auto & tmp_bucket  = buckets[index];
    for (size_t i = 0; i < bucket_size; ++i) {
      if (buckets[index][i].first == key) {

        values[val] = buckets[index][i].second;

        return;
      }
    }

    /* if there is no element with specified key, insert new value
     * to the end of values vector and put reference in proper
     * bucket transactionally */

    values.push_back(val);

    bucket_elem_type tmp_pair(std::move(key_type(key)),
                              std::move(values.size() - 1));
    buckets[index].push_back(std::move(tmp_pair));
    snapshot(this->region, region_sz_bytes, MS_SYNC);
  }
};
