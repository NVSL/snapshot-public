// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   error.hh
 * @date   March 27, 2021
 * @brief  Brief description here
 */

#pragma once

#include <cstring>
#include <iostream>
#include <optional>

#include "nvsl/error.hh"

#define PERROR_FATAL(msg)                       \
  do {                                          \
    std::string msg_str(msg);                   \
    perror(msg_str.c_str());                    \
    exit(1);                                    \
  } while (0);
