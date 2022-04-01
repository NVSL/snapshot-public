// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   recovery.hh
 * @date   mars 29, 2022
 * @brief  Brief description here
 */

#pragma once

#include <filesystem>

namespace fs = std::filesystem;

namespace nvsl {
  namespace cxlbuf {
    /**
     * @brief Class to handle all the cxlbuf operations for a pmem file
     */
    class PmemFile {
    private:
      fs::path path;
      void *addr;
      size_t len;

      /**
       * @brief Check if the file needs recovery
       * @param[in] path Path to the original file
       * @return Returns true if the file needs recovery
       */
      bool needs_recovery();

      /**
       * @brief Recover all the outstanding logs in the file
       * @param[in] path Path to the original file
       */
      void recover();

      void create_backing_file_internal();
      bool has_backing_file();
      std::string get_backing_fname() const;

    public:
      /**
       * @brief Try recovery and creating backing files
       */
      PmemFile(const fs::path &path, void* addr, size_t len);
      void create_backing_file();
      void map_backing_file();
      void *map_to_page_cache(int flags, int prot, int fd, off_t off);
      void set_addr(void* addr);
    };
  } // namespace cxlbuf
} // namespace nvsl
    
