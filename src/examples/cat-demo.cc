// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   cat-demo.cc
 * @date   février  6, 2022
 * @brief  Brief description here
 */

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <list>
#include <numeric>
#include <signal.h>
#include <stdint.h>
#include <sys/prctl.h>
#include <unistd.h>

const size_t SCRATCH_SZ = (10*1024*1024)/sizeof(uint64_t);

uint64_t scratch[SCRATCH_SZ];
constexpr uint64_t SZ_2MB = (uint64_t)(512*1024)/sizeof(uint64_t);

int main() {
  int pid = fork();

  if (pid == 0) {
    std::cout << "Checking latency from pid " << getpid() << std::endl;

    std::memset(scratch, '0', sizeof(scratch));
    float
      min = std::numeric_limits<float>::max(), 
      max = std::numeric_limits<float>::min(),
      rolling_avg = 0;
    const size_t WINDOW_SZ = 10;
    std::list<float> latencies;
    while (true) {
      using hrc = std::chrono::high_resolution_clock;
      usleep(50000);
      
      auto start = hrc::now();
      volatile int result = 0;
      for (uint64_t i = 0; i < SZ_2MB; i++) {
        result = result + (int)scratch[rand()%SZ_2MB];
      }
      
      auto end = hrc::now();
      auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count()/float(SZ_2MB);
      int pid = getpid();
      
      latencies.push_back(latency);
      min = *std::min_element(latencies.begin(), latencies.end());
      max = *std::max_element(latencies.begin(), latencies.end());
      rolling_avg = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();

      int cpuid = sched_getcpu();
      
      std::cout << "Latency [" << pid << "] [cpu" << cpuid << "] "
                 << std::setw(10) << latency << " ns min = " << std::setw(10) << min << " ns max = " << std::setw(10) << max
                << " ns rolling avg = " << std::setw(10) << rolling_avg << std::endl;
      if (latencies.size() > WINDOW_SZ) {
        latencies.pop_front();
      }
      
    }
  } else if (pid != -1) {
    int i = 0;
    while (i++ < 15) {
      if (fork() == 0) {
        std::cout << "[" << i << "] Waiting for 5s before thrashing cache." << std::endl;
        sleep(5);
        std::cout << "Thrashing cache from pid " << pid << std::endl;
        std::cout << "sizeof(scratch) = " << sizeof(scratch)/float(1024*1024) << " MiB" << std::endl;

        std::memset(scratch, (uint8_t)rand(), sizeof(scratch));
        while (true) {
          volatile int result = 0;
          // for (uint64_t i = 0; i < SCRATCH_SZ; i++) {
          result = result + (int)scratch[rand()%SCRATCH_SZ];
          // }
        }
      }
    }
    prctl(PR_SET_PDEATHSIG, SIGHUP);
    while (true);

  } else {
    perror("Fork");
    exit(1);
  }


}
