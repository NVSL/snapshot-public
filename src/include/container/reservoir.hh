// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   reservoir.hh
 * @date   mars 14, 2022
 * @brief  Brief description here
 */

#pragma once

#include <boost/interprocess/indexes/iset_index.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/mem_algo/rbtree_best_fit.hpp>
#include <boost/interprocess/mem_algo/simple_seq_fit.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/mutex_family.hpp>

#include "nvsl/error.hh"

#include <cassert>
#include <iostream>
#include <sys/mman.h>
#include <unistd.h>

namespace bip = boost::interprocess;

// typedef bip::rbtree_best_fit<bip::interprocess_mutex> null_mutex_ator;
// typedef bip::basic_managed_mapped_file<char, null_mutex_ator,
// bip::iset_index> mmf;
typedef bip::managed_mapped_file mmf;

namespace libpuddles {
  class reservoir_t {
  private:
    mmf *ator;
    int fd;

  public:
    reservoir_t(const std::string &fname, const size_t bytes) {
      ator = new mmf(bip::open_or_create, fname.c_str(), bytes);
      fd = open(fname.c_str(), O_RDWR);
    }

    template <typename T>
    T *allocate_root() {
      return (T *)ator->construct<T>("root")();
    }

    template <typename T>
    T *get_root() {
      return (T *)ator->find<T>("root").first;
    }

    template <typename T>
    T *malloc(const size_t count = 1) {
      return (T *)ator->allocate(sizeof(T) * count);
    };

    void free(void *ptr) { ator->deallocate(ptr); }

    int get_fd() const { return fd; }

    void sync() { msync(ator->get_address(), ator->get_size(), MS_SYNC); }

    void madvise_4k_page() {
      int ret = madvise(ator->get_address(), ator->get_size(), MADV_NOHUGEPAGE);
      if (ret == -1) {
        DBGE << "Unable to set 4k page for the allocator\n";
        DBGE << PSTR() << "\n";
        exit(1);
      }
    }
  };
} // namespace libpuddles
