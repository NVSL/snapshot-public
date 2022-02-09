// -*- mode: c++; c-basic-offset: 2; -*-

#pragma once

/**
 * @file   constants.hh
 * @date   May 21, 2021
 * @brief  Brief description here
 */

#include <cstddef>
#include <cstdint>

namespace common {
  /** @brief Enum to help with size of things */
  enum SZ : size_t
  {
    B = 1,
    KiB = 1024 * B,
    MiB = 1024 * KiB,
    GiB = 1024 * MiB,
    TiB = 1024 * GiB
  };

  enum time_unit
  {
    s_unit = 0,
    ms_unit,
    us_unit,
    ns_unit,
    any_unit
  };

  static constexpr size_t PAGE_SIZE = 4 * SZ::KiB;      // Bytes
  static constexpr size_t HUGE_PAGE_SIZE = 2 * SZ::MiB; // Bytes
  static constexpr size_t CL_SIZE = 64 * SZ::B;         // Bytes


  enum class pkg_type_t
  {
    TAR_GZ = 0,
    DIR = 1
  };
} // namespace common

