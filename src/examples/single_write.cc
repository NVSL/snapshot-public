// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   single_write.cc
 * @date   avril  2, 2022
 * @brief  Brief description here
 */

#include <boost/interprocess/indexes/iset_index.hpp>
#include <boost/interprocess/interprocess_fwd.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/mem_algo/rbtree_best_fit.hpp>
#include <boost/interprocess/sync/mutex_family.hpp>
#include <boost/interprocess/mem_algo/simple_seq_fit.hpp>
#include <stdint.h>
#include <iostream>

namespace bip = boost::interprocess;

typedef bip::rbtree_best_fit<bip::null_mutex_family> null_mutex_ator;
typedef bip::basic_managed_mapped_file<char, null_mutex_ator, bip::iset_index>
mmf;

extern bool startTracking;

int main() {
  // auto res = new mmf(bip::open_or_create, "/mnt/pmem0/single_write",
                     // 1024*1024);
  
  
  int fd = open("/mnt/pmem0/single_write", O_RDWR | O_CREAT, 0600);
  lseek(fd, 1024*1024, SEEK_SET);
  write(fd, "\0", 1);
  void *addr = mmap(nullptr, 1024*1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  startTracking = true;
  // auto root_ptr = res->find<uint64_t>("root").first;
  // if (root_ptr == nullptr) {
  //   std::cerr << "Writing value 100" << std::endl;
  //   root_ptr = res->construct<uint64_t>("root")();
  //   *root_ptr = 100;
  // } else {
  //   std::cerr << "Read value " << *root_ptr << std::endl;;
  // }

  auto root_ptr = (uint64_t*)addr;
  std::cerr << "Read value " << *root_ptr << std::endl;;
  std::cerr << "Writing value 100" << std::endl;
  *root_ptr = 100;

  msync(root_ptr, 1024*1024, MS_SYNC);


  exit(0);
}
