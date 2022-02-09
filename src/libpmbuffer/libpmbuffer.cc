// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   libpmbuffer.cc
 * @date   février  8, 2022
 * @brief  Brief description here
 */

#include "libpmbuffer.hh"
#include "envvars.hh"
#include "error.hh"

#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

constexpr size_t PERST_BUF_SZ = 2.5*1024*1024;

static char *perst_buf;

void mount_perst_buf() {
  std::string perst_buf_fname = get_env_str(PERST_BUF_LOC_ENV);
  int perst_buf_fd = open(perst_buf_fname.c_str(), O_RDWR);
  perst_buf = (char*)mmap(NULL, PERST_BUF_SZ, PROT_READ | PROT_WRITE,
                          MAP_SHARED_VALIDATE | MAP_SYNC, perst_buf_fd, 0);

  if (perst_buf == (char*)-1) {
    ERROR("Unable to mmap the PM Buffer at location " + perst_buf_fname);
  }
}

__attribute__((constructor))
static void libpmbuffer_ctor() {
  
  
  // perst_buf = (char*)mmap();
  
  // memset(&perst_buf, 0, PERST_BUF_SZ);
}

