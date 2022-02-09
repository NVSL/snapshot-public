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

#include "common.hh"
#include "envvars.hh"
#include "trace.hh"

static inline std::string print_stuff__(std::string msg, std::string dec) {
  std::cout << msg << " -- " << dec << std::endl;
  return msg;
}

#define PERROR_FATAL(msg)                       \
  do {                                          \
    std::string msg_str(msg);                   \
    perror(msg_str.c_str());                    \
    exit(1);                                    \
  } while (0);

/** @brief Convert errno to exception */
#define PERROR_EXCEPTION(errcode, msg)                         \
  ([]() { DBGE << msg << std::endl; }(),                       \
   common::print_trace(),                                      \
   std::system_error(errcode, std::generic_category()))

/** @brief Throw exception with msg if val is NULL */
#define ASSERT_NON_NULL(val, msg)      \
  do {                                 \
    DBGE << msg << std::endl;          \
    print_trace();                     \
    if ((val) == NULL) {               \
      throw std::runtime_error((msg)); \
    }                                  \
  } while (0);

#ifdef RELEASE
/** @brief Throw exception with msg if val is NULL */
#define ERROR(msg)         \
  do {                        \
    DBGE << msg << std::endl; \
    exit(1);                  \
  } while (0);
#else
/** @brief Throw exception with msg if val is NULL */
#define ERROR(msg)                                    \
  do {                                                   \
    DBGE << msg << std::endl;                            \
    if (not get_env_val(NO_STACKTRACE_ENV)) { \
      common::print_trace();                             \
    }                                                    \
    exit(1);                                             \
  } while (0);
#endif // RELEASE

/** @brief Exit with no debugging info */
#define ERROR_CLEAN(msg)   \
  do {                        \
    DBGE << msg << std::endl; \
    exit(1);                  \
  } while (0);

/** @brief Assert a condition w/ msg and generate backtrace on fail */
#ifndef NDEBUG
#define ASSERT(cond, msg)                                       \
  if (!(cond)) [[unlikely]] {                                      \
    DBGE << __FILE__ << ":" << __LINE__ << " Assertion `" << #cond \
         << "' failed: " << msg << std::endl;                      \
    if (not get_env_val(NO_STACKTRACE_ENV)) {           \
      common::print_trace();                                      \
    }                                                              \
    exit(1);                                                       \
  }
#else
#define ASSERT(cond, msg) \
  if (true) {                \
    (void)(cond);            \
  }
#endif

/** @brief Convert errno to std::string */
static inline std::string PSTR() {
  return std::string(__FILE__) + ":" + std::to_string(__LINE__) + " " +
         std::string(strerror(errno));
}

using std::make_optional;
using std::optional;
