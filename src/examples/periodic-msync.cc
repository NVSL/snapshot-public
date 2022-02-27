#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <filesystem>
#include <fcntl.h>
#include <chrono>
#include <cstring>
#include <unistd.h>
#include <vector>
#include <xmmintrin.h>
#include <immintrin.h>

using hrc = std::chrono::high_resolution_clock;
constexpr size_t MAP_SIZE = 100*1024*1024; // 100 MiB

char *buf, *vol_buf, *pm_buf;

void us_msync(void *addr, size_t bytes) {
  std::memcpy(pm_buf, vol_buf, bytes);
  for (size_t cur_b = 0; cur_b < bytes; cur_b+=64) {
    _mm_clwb((void*)((char*)pm_buf+cur_b));
  }
  _mm_sfence();
}

void flush_locations(const std::vector<void*> &locs) {
  // std::memcpy(pm_buf, vol_buf, bytes);
  // for (size_t cur_b = 0; cur_b < bytes; cur_b+=64) {
  //   _mm_clwb((void*)((char*)pm_buf+cur_b));
  // }
  // _mm_sfence();
}

int main(int argc, char *argv[]) {
  if (argc != 4) {
    fprintf(stderr, "USAGE:\n\t%s <path to buffer> <path to backing PM file> <update count>\n",
            argv[0]);
    exit(1);
  }

  size_t update_cnt = std::stoull(argv[3]);

  /* File opened on PM using mmap and flushed using msync */
  std::string fname = std::string(argv[1]);
  int fd = open(fname.c_str(), O_RDWR);
  if (fd == -1) {
    perror("open failed");
    exit(1);
  }

  /* File opened on PMEM and written to from an anonymous mmap using memcpy */
  std::string pm_fname = std::string(argv[2]);
  int fd_pm = open(pm_fname.c_str(), O_RDWR);
  if (fd_pm == -1) {
    perror("open failed");
    exit(1);
  }
  
  buf = (char*)mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (buf == (void*)-1) {
    perror("mmap failed for buf");
    exit(1);
  }
  vol_buf = (char*)mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
  if (vol_buf == (void*)-1) {
    perror("mmap failed for vol_buf");
    exit(1);
  }
  pm_buf = (char*)mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SYNC | MAP_SHARED_VALIDATE, fd_pm, 0);
  if (pm_buf == (void*)-1) {
    perror("mmap failed for pm_buf");
    exit(1);
  }
  

  size_t iter = 0;
  auto begin = hrc::now();
  while (iter++ < 4096) {
    size_t wr_cnt = 0;
    while (wr_cnt++ < update_cnt) {
      buf[rand()%MAP_SIZE] = rand();
    }
    msync(buf, MAP_SIZE, MS_SYNC);
  }
  auto msync_time = (hrc::now()-begin).count();

  iter = 0;
  begin = hrc::now();
  while (iter++ < 4096) {
    size_t wr_cnt = 0;
    while (wr_cnt++ < update_cnt) {
      vol_buf[rand()%MAP_SIZE] = rand();
    }
    us_msync(vol_buf, MAP_SIZE);
  }
  auto no_msync_time = (hrc::now()-begin).count();

  // std::cout << "update count,msync,memcpy and flush+sfence" << "\n";
  std::cout << update_cnt << "," << msync_time << ","
            << no_msync_time << "\n";

  // system(("cat /proc/" + std::to_string(getpid()) + "/maps").c_str());
  
  return 0;
}
