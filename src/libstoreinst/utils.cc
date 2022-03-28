// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   utils.cc
 * @date   mars 28, 2022
 * @brief  Brief description here
 */

#include "libstoreinst.hh"
#include "utils.hh"

bool addr_in_range(void *addr) {
  if (start_addr <= addr and addr <= end_addr) {
    return true;
  } else {
    return false;
  }
}
