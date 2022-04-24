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

int main() {
  constexpr size_t alloc = 1UL * 128 * 1024 * 1024;
  auto fb_ptr = nvsl::libvram::malloc(alloc);

  nvsl::PMemOps *pmemops = new nvsl::PMemOpsClwb;

  //  nvsl::memcheck(fb_ptr, alloc >> 8);

  size_t val = 0;
  nvsl::Clock clk;
  auto fb_bptr = (char *)fb_ptr;

  clk.reset();
  clk.tick();
  for (size_t j = 0; j < 1; j++) {
    for (size_t i = 0; i < alloc; i++) {
      fb_bptr[i] = 1;
      pmemops->flush(&fb_bptr[i], 1);
      pmemops->drain();
    }
  }
  clk.tock();

  std::cout << "Write took " << clk.ns() / (double)(alloc) << ". Val = " << val
            << std::endl;

  clk.reset();
  clk.tick();
  for (size_t j = 0; j < 1; j++) {
    for (size_t i = 0; i < alloc; i++) {
      val += fb_bptr[i];
    }
  }
  clk.tock();

  printf("done with the first loop\n");

  sleep(1);
  val = 0;

  clk.reset();
  clk.tick();
  for (size_t j = 0; j < 1; j++) {
    for (size_t i = 0; i < alloc; i++) {
      val += fb_bptr[i];
    }
  }
  clk.tock();

  //  memset(fb_bptr, 0, alloc);

  std::cout << "Read took " << clk.ns() / (double)(alloc) << ". Val = " << val
            << std::endl;

  std::cout << "Total loops = " << alloc * 2 << std::endl;
}
