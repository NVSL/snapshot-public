#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <filesystem>
#include <fcntl.h>
#include <chrono>
#include <cstring>
#include <xmmintrin.h>

using hrc = std::chrono::high_resolution_clock;
constexpr size_t MAP_SIZE = 100*1024; // 100 MiB

char *buf, *pm_buf;

void us_msync(void *addr, size_t bytes) {
  std::memcpy(pm_buf, buf, bytes);
  _mm_sfence();
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "USAGE:\n\t%s <path to buffer> <path to backing PM file>\n",
            argv[0]);
    exit(1);
  }
  
  std::string fname = std::string(argv[1]);
  int fd = open(fname.c_str(), O_RDWR);
  if (fd == -1) {
    perror("open failed");
    exit(1);
  }
  
  std::string pm_fname = std::string(argv[1]);
  int fd_pm = open(pm_fname.c_str(), O_RDWR);
  if (fd == -1) {
    perror("open failed");
    exit(1);
  }
  
  buf = (char*)mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (buf == (void*)-1) {
    perror("mmap failed for buf");
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
    while (wr_cnt++ < 4096) {
      buf[rand()%MAP_SIZE] = rand();
    }
    msync(buf, MAP_SIZE, MS_SYNC);
  }
  auto msync_time = (hrc::now()-begin).count();

  iter = 0;
  begin = hrc::now();
  while (iter++ < 4096) {
    size_t wr_cnt = 0;
    while (wr_cnt++ < 4096) {
      buf[rand()%MAP_SIZE] = rand();
    }
    us_msync(buf, MAP_SIZE);
  }
  auto no_msync_time = (hrc::now()-begin).count();

  std::cout << msync_time << "\n"
            << no_msync_time << "\n";

  return 0;
}
