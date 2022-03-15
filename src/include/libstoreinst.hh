// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   libstoreinst.hh
 * @date   mars 14, 2022
 * @brief  Brief description here
 */

#pragma once
#include <unistd.h>

extern "C" {
  void *memcpy(void *__restrict dst, const void *__restrict src, size_t n)
    __THROW;

  void *memmove(void *__restrict dst, const void *__restrict src, size_t n)
    __THROW;

  extern bool startTracking;
  int snapshot(void *addr, size_t length, int flags);

  void libstoreinst_ctor();
}
