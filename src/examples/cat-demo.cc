// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   cat-demo.cc
 * @date   février  6, 2022
 * @brief  Brief description here
 */

#include "common.hh"
#include "libpmbuffer.hh"
#include "nvsl/trace.hh"
#include "nvsl/common.hh"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <list>
#include <numeric>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <sys/prctl.h>
#include <sys/mman.h>
#include <unistd.h>

const size_t SCRATCH_SZ = (10*1024*1024)/sizeof(uint64_t);

uint64_t scratch[SCRATCH_SZ];
uint64_t *reserved_scratch;

constexpr uint64_t SZ_2MB = (uint64_t)(6*1024 * 1024) / sizeof(uint64_t);
constexpr uint64_t MAX_ITER = 64;
constexpr uint64_t WARM_UP_ITER = 16;

void sa(int signal, siginfo_t *si, void *arg) {
  DBGE << "Caught segfault" << std::endl;
  DBGE << "Stacktrace: \n" << nvsl::get_stack_trace() << std::endl;
  exit(0);
}

int main(int argc, char* argv[]) {
  struct sigaction act;

  memset(&act, 0, sizeof(struct sigaction));
  sigemptyset(&act.sa_mask);
  act.sa_sigaction = sa;
  act.sa_flags   = SA_SIGINFO;

  sigaction(SIGSEGV, &act, NULL);

  if (argc != 2) {
    std::cerr << "USAGE:\n\t" << argv[0] << " <cpuid>\n";
    exit(1);
  }

  int cpuid = atoi(argv[1]);

  for (size_t i = 0; i < SCRATCH_SZ; i++) {
    scratch[i] = rand()%SZ_2MB;
  }
  
  float
    min = std::numeric_limits<float>::max(), 
    max = std::numeric_limits<float>::min(),
    avg = 0;
  size_t counts = 0;

  constexpr size_t TRASHING_THR_CNT = 15;
  int pids[TRASHING_THR_CNT];
  size_t i = 0;
  while (i < TRASHING_THR_CNT) {
    int pid = fork();
    if (pid == 0) {
      common::bindcore(cpuid+i+1);
      memset(&scratch, (char)rand(), sizeof(scratch));
      while (true) {
        volatile uint64_t result = 0;
        scratch[rand()%(SZ_2MB)] = rand();
        result = result + scratch[rand()%(SZ_2MB)];
      }
    }
    pids[i] = pid;
    i++;
  }

  /* Bind to the requested CPU core */
  common::bindcore(cpuid);
  std::cout << getpid() << "," << sched_getcpu() << std::endl;
    
  size_t iter = 0;
  
  auto pmbuf = pmbuffer::get_pmbuffer();
  DBGH(0) << "Init reserved scratch" << std::endl;
  reserved_scratch = (uint64_t*)pmbuf->raw;
  DBGH(0) << "Got the persistent buffer at " << (void*)reserved_scratch
          << std::endl;
  for (size_t i = 0; i < pmbuf->bytes/sizeof(uint64_t); i++) {
    reserved_scratch[i] = rand()%SZ_2MB;
  }
  DBGH(0) << "Init done" << std::endl;

  
  while (iter++ < MAX_ITER) {
    using hrc = std::chrono::high_resolution_clock;
      
    auto start = hrc::now();
    for (uint64_t i = 0; i < SZ_2MB; i++) {
      auto scratch = reserved_scratch;
      __attribute__((unused)) volatile auto result = scratch[rand()%SZ_2MB];
    }
    auto end = hrc::now();
    
    auto latency = std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count()/float(3*SZ_2MB);


    if (iter > WARM_UP_ITER) {
      min = min < latency ? min : latency;
      max = max < latency ? max : latency;
      avg = ((avg * counts) + latency)/float(counts+1);
      counts++;
    }
  }

  std::cout << min << "," << max << "," << avg << std::endl;

  for (size_t i = 0; i < TRASHING_THR_CNT; i++) {
    kill(pids[i], SIGTERM);
  }
}
