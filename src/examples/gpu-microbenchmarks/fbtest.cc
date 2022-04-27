#include "nvsl/clock.hh"
#include "nvsl/pmemops.hh"
#include "nvsl/utils.hh"
#include <fcntl.h>
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "libvram/libvram.hh"

constexpr size_t cl_align(size_t val) {
  return (val >> 6) << 6;
}

int main() {
  constexpr size_t alloc = 4UL * 1024 * 1024 * 1024;
  constexpr size_t random_ops = 8UL * 1024 * 1024;
  constexpr size_t seq_ops = 32UL * 1024 * 1024;

  ctor_libvram();
  auto fb_ptr = nvsl::libvram::malloc(alloc);

  nvsl::PMemOps *pmemops = new nvsl::PMemOpsClwb;

  //  nvsl::memcheck(fb_ptr, alloc >> 8);

  size_t val = 0;
  nvsl::Clock clk;
  auto fb_wptr = (uint64_t *)fb_ptr;

  clk.reset();
  clk.tick();
  for (size_t j = 0; j < 1; j++) {
    for (size_t i = 0; i < random_ops; i++) {
      const auto idx = cl_align(rand() % alloc) / 8;
      fb_wptr[idx] = 1;
      pmemops->flush(&fb_wptr[idx], 8);
      pmemops->drain();
    }
  }
  clk.tock();

  std::cout << "Random write took " << clk.ns() / (double)(random_ops)
            << ". Val = " << val << std::endl;

  clk.reset();
  clk.tick();
  for (size_t j = 0; j < 1; j++) {
    for (size_t i = 0; i < random_ops; i++) {
      const auto idx = cl_align(rand() % alloc) / 8;
      val += fb_wptr[idx];
    }
  }
  clk.tock();

  std::cout << "Random read took " << clk.ns() / (double)(random_ops)
            << ". Val = " << val << std::endl;

  clk.reset();
  clk.tick();
  for (size_t j = 0; j < 1; j++) {
    for (size_t i = 0; i < seq_ops; i++) {
      const size_t idx = i * 8;
      val += fb_wptr[idx];
    }
  }
  clk.tock();

  std::cout << "Sequential read took " << clk.ns() / (double)(seq_ops)
            << ". Val = " << val << std::endl;

  clk.reset();
  clk.tick();
  for (size_t j = 0; j < 1; j++) {
    for (size_t i = 0; i < seq_ops; i++) {
      const size_t idx = rand() % 8;
      val += fb_wptr[idx];
    }
  }
  clk.tock();

  std::cout << "Cache local read took " << clk.ns() / (double)(seq_ops)
            << ". Val = " << val << std::endl;

  clk.reset();
  clk.tick();
  for (size_t j = 0; j < 1; j++) {
    for (size_t i = 0; i < seq_ops; i++) {
      const size_t idx = i * 8;
      fb_wptr[idx] = idx;
      pmemops->flush(&fb_wptr[idx], 8);
      pmemops->drain();
    }
  }
  clk.tock();

  std::cout << "Sequential write took " << clk.ns() / (double)(seq_ops)
            << ". Val = " << val << std::endl;

  std::cout << "Total loops = " << alloc * 2 << std::endl;
}
