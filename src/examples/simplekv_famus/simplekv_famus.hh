// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019, Intel Corporation */

/*
 * simplekv.hpp -- implementation of simple kv which uses vector to hold
 * values, string as a key and array to hold buckets
 */
#include <cstddef>
#include <iostream>
#include <linux/fs.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <vector>

// #include "libstoreinst.hh"
#include "container/pair.hh"
#include "container/string.hh"
#include "container/vector.hh"
#include "nvsl/clock.hh"
#include "nvsl/common.hh"
#include "nvsl/error.hh"
#include "nvsl/string.hh"
#include "nvsl/utils.hh"

static inline int rwperm(mode_t m, unsigned int r, unsigned int w) {
  return (!!(m & S_IRUSR) == r) && (!!(m & S_IWUSR) == w);
}

/**
 * @note From: Terence Kelly, "Good Old-Fashioned Persistent Memory", USENIX
 * ;login; Winter 2019, Vol. 44, No. 4.
 *
 * We must fsync() backing file twice to ensure that snapshot data are durable
 * before success indicator (file permission) becomes durable.  We're not using
 * fallocate() to reserve space for worst-case scenario in which backing file
 * and snapshot file diverge completely, because that could defeat the reflink
 * sharing that makes snapshots efficient; read "man ioctl_ficlone".  The
 * "ioctl(FICLONE)" works only on reflink-enabled file systems, e.g., BtrFS,
 * XFS, OCFS2. */
extern int snap_fd;
// int dir_fd = -1;
inline int famus_snap_sync(int bfd) {
  struct stat sb;

  if (snap_fd == -1) {
    const std::string bfname = nvsl::fd_to_fname(bfd);
    const std::string snapshot_file = bfname + ".snapshot";

    snap_fd = open(snapshot_file.c_str(), O_CREAT | O_RDWR, 0666);
    if (snap_fd == -1) {
      DBGE << "Unable to open fd for the snapshot file " << snapshot_file
           << "\n";
      DBGE << PSTR() << "\n";
      exit(1);
    }
  }

  auto fatal_if = [&](bool cond, std::string fname, int line) {
    if (cond) {
      DBGE << "Failed at " << fname << ":" << line << "\n";
      DBGE << PSTR() << "\n";
      exit(1);
    }
  };

  nvsl::Clock clk;

  clk.tick();
  fatal_if((0 != fstat(snap_fd, &sb)), __FILE__, __LINE__);
  clk.tock();
  std::cerr << "fstat took = " << clk.ns() / 1000.0 << " us\n";
  // fatal_if((!rwperm(sb.st_mode, 0, 1)), __FILE__, __LINE__);

  clk.reset();
  clk.tick();
  fatal_if((0 != ioctl(snap_fd, FICLONE, bfd)), __FILE__, __LINE__);
  clk.tock();
  std::cerr << "ioctl took = " << clk.ns() / 1000.0 << " us\n";

  clk.reset();
  clk.tick();
  fatal_if((0 != fsync(snap_fd)), __FILE__, __LINE__);
  clk.tock();
  std::cerr << "fsync took = " << clk.ns() / 1000.0 << " us\n";

  clk.reset();
  clk.tick();
  fatal_if((0 != fchmod(snap_fd, S_IRUSR)), __FILE__, __LINE__);
  clk.tock();
  std::cerr << "fchmod took = " << clk.ns() / 1000.0 << " us\n";

  clk.reset();
  clk.tick();
  fatal_if((0 != fstat(snap_fd, &sb)), __FILE__, __LINE__);
  clk.tock();
  std::cerr << "fstat took = " << clk.ns() / 1000.0 << " us\n";

  clk.reset();
  clk.tick();
  fatal_if((!rwperm(sb.st_mode, 1, 0)), __FILE__, __LINE__);
  fatal_if((0 != fsync(snap_fd)), __FILE__, __LINE__);
  fatal_if((0 != lseek(snap_fd, 0, SEEK_SET)), __FILE__, __LINE__);
  // fatal_if((0 < dir_fd && 0 != fsync(dirfd)), __FILE__, __LINE__);
  // fatal_if((0 != close(snap_fd)), __FILE__, __LINE__);
  return 0;
}

/**
 * Value - type of the value stored in hashmap
 * N - number of buckets in hashmap
 */
template <typename Value, std::size_t N>
class simple_kv {
private:
  using key_type = libpuddles::obj::string;
  using bucket_elem_type = libpuddles::obj::pair<key_type, std::size_t>;
  using bucket_type = libpuddles::obj::vector<bucket_elem_type>;
  using value_vector = libpuddles::obj::vector<Value>;

  bucket_type buckets[N];
  value_vector values;

  reservoir_t *res;

public:
  simple_kv() = default;

  void init(reservoir_t *res, size_t region_sz_bytes) {
    this->res = res;

    for (size_t i = 0; i < N; ++i) {
      buckets[i].init(res);
      // new (&buckets[i]) bucket_type;
    }

    values.init(res);
    // new (&values) value_vector;
  }

  const Value &get(const std::string &key) const {
    // std::cout << "Value at " << buckets[8717][58].first << std::endl;

    auto index = std::hash<std::string>{}(key) % N;

    size_t bucket_size = buckets[index].size();
    auto &tmp_bucket = buckets[index];

    // std::cout << "bucket.size = " << tmp_bucket.size() << std::endl;

    for (size_t i = 0; i < bucket_size; ++i) {
      auto &key_pair = tmp_bucket[i];
      // std::cout << "\tkey = " << key_pair.first << std::endl;

      if (key_pair.first == key) return values[key_pair.second];
    }

    // std::cout << "No element found in index " << index << std::endl;

    throw std::out_of_range("no entry in simplekv");
  }

  void put(const std::string &key, const Value &val, bool persist = true) {
    // bool tgtKey = false;
    // if (key == "user932529841982028274") {
    //   tgtKey = true;
    //   std::cout << "putting key user932529841982028274" << std::endl;
    // }

    auto index = std::hash<std::string>{}(key) % N;

    // if (index == 8717 and buckets[index].size() >= 58) {
    //   std::cout << "Inserting key " << key << std::endl;
    //   std::cout << "Value at " << buckets[8717][58].first << std::endl;
    // }

    /* search for element with specified key - if found
     * transactionally update its value */
    size_t bucket_size = buckets[index].size();
    // auto & tmp_bucket  = buckets[index];
    for (size_t i = 0; i < bucket_size; ++i) {
      if (buckets[index][i].first == key) {
        // if (tgtKey)
        //   std::cout << "found key existsing" << std::endl;
        values[val] = buckets[index][i].second;

        if (persist) {
#ifndef DONT_SYNC
          famus_snap_sync(res->get_fd());
#endif
        }

        return;
      }
    }

    /* if there is no element with specified key, insert new value
     * to the end of values vector and put reference in proper
     * bucket transactionally */

    values.push_back(res, val);

    bucket_elem_type tmp_pair(res, std::move(key_type(res, key)),
                              std::move(values.size() - 1));
    // size_t pos = buckets[index].size();
    buckets[index].push_back(res, std::move(tmp_pair));

    // if (tgtKey) {
    //   std::cout << "PUT to index " << index << " pos = "<< pos << std::endl;
    //   std::cout << buckets[index][pos].first<< std::endl;
    // }
    if (persist) {
#ifndef DONT_SYNC
      famus_snap_sync(res->get_fd());
#endif
    }
  }
};
