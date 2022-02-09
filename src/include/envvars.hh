// -*- mode: c++; c-basic-offset: 2; -*-

#pragma once

/**
 * @file   envvars.hh
 * @date   January 21, 2021
 * @brief  Brief description here
 */

#include <string>

#define DECL_ENV(name) \
  static const char *name##_ENV __attribute__((unused)) = (char *)(#name)

DECL_ENV(PERST_BUF_LOC);
DECL_ENV(NO_STACKTRACE);
DECL_ENV(LOG_WILDCARD);
DECL_ENV(BIND_CORE);

/**
 * @brief Looks up a boolean like env var
 * @details Behavior:
 * 1. Env variable missing -> return false
 * 2. 0 -> return false
 * 1. 1 -> return true
 */
static inline bool get_env_val(const std::string var) {
  bool result = false;

  const char *val = std::getenv(var.c_str());
  if (val != nullptr) {
    const std::string val_str = std::string(val);

    if (val_str == "1") {
      result = true;
    }
  }

  return result;
}

/**
 * @brief Looks up a string value from env var
 * @details Behavior:
 * 1. Env variable missing -> empty string
 * @param[in] var Environment variable's name
 * @param[in] def Default value to return if unset
 */
static inline std::string get_env_str(const std::string var, 
                                      const std::string def = "") {
  std::string result = def;

  const char *val = std::getenv(var.c_str());
  if (val != nullptr) {
    result = std::string(val);
  }

  return result;
}
