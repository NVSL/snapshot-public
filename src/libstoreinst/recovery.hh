// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   recovery.hh
 * @date   mars 29, 2022
 * @brief  Brief description here
 */

#pragma once

#include <filesystem>

namespace fs = std::filesystem;

/**
 * @brief Check if the file needs recovery
 * @param[in] path Path to the original file
 * @return Returns true if the file needs recovery
 */
bool needs_recovery(const fs::path &fpath);

/**
 * @brief Recover all the outstanding logs in the file
 * @param[in] path Path to the original file
 */
void recover(const fs::path &fpath);
