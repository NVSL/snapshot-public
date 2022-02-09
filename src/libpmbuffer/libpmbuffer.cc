// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   libpmbuffer.cc
 * @date   février  8, 2022
 * @brief  Brief description here
 */

#include "libpmbuffer.hh"
#include "envvars.hh"
#include "error.hh"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace fs = std::filesystem;

constexpr size_t PERST_BUF_SZ = 2.5*1024*1024;

static char *perst_buf;

/** 
 * @brief Allocate the persist buffer at the specified path if absent
 * @param[in] path Path to the file
 * @param[in] bytes File size
 */
void perst_buf_fallocate(fs::path path, size_t bytes) {
  if (not  fs::is_regular_file(path)) {
    int fd = open(path.c_str(), O_CREAT | O_RDWR, 0644);
    if (fd == -1) PERROR_FATAL("Unable to open persistent buffer backing file");

    if (bytes-1 != (size_t)lseek(fd, bytes-1, SEEK_SET))
      PERROR_FATAL("Unable to seek in the persistent buffer backing file");

    char buf = '\0';
    if (1 != write(fd, &buf, 1))
      PERROR_FATAL("Unable to write persistent buffer backing file");
  }
  
}

/** @brief Mount the persistent buffer using its path */
void mount_perst_buf(fs::path perst_buf_fname) {
  if (perst_buf_fname.empty()) {
    fprintf(stderr, "No persist buffer name specified, check README.\n");
    exit(1);
  }
  int perst_buf_fd = open(perst_buf_fname.c_str(), O_RDWR);

  if (perst_buf_fd == -1) {
    perror("Unable to open persistent buffer backing file");
    exit(1);
  }
  
  perst_buf = (char*)mmap(NULL, PERST_BUF_SZ, PROT_READ | PROT_WRITE,
                          MAP_SHARED_VALIDATE | MAP_SYNC, perst_buf_fd, 0);
  
  if (perst_buf == (char*)-1) {
    fprintf(stderr, "Unable to mmap the PM Buffer at location %s\n.",
            perst_buf_fname.c_str());
  }
}

/**
 * @brief Reserve cache for a core
 * @param[in] cpuid zero-indexed CPU id to reserve the cache for
 * @param[in] ways Number of ways to reserve
 * @todo Fix this to use libpqos
 */
void reserve_cache(size_t cpuid, size_t ways) {
  /* COS0 -> 0x7f0 and COS1 -> 0x00f */
  /* Core 0 -> COS1 and Core (!0) -> COS0 */
  std::string cmd = "pqos -e \"llc@0:1=0xf;llc@0:0=0x7f0\" -a \"cos:1=0;cos:0=1-19;\"";
  system(cmd.c_str());
}

__attribute__((constructor))
static void libpmbuffer_ctor() {
  fs::path perst_buf_fname = get_env_str(PERST_BUF_LOC_ENV);

  perst_buf_fallocate(perst_buf_fname, PERST_BUF_SZ);
  mount_perst_buf(perst_buf_fname);

  int pid = fork();

  if (pid == 0) {
    /* Use a process to create a cache reservation */
    pid = getpid();

    int cpuid = std::stoul(get_env_str(BIND_CORE_ENV, "0"));
    common::bindcore(cpuid);

    reserve_cache(cpuid, 0xf);

    memset(perst_buf, 0, PERST_BUF_SZ);
    
    /* We don't have anything else to do, put this process to sleep */
    pause();
  }
  
}

