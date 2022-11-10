// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   blkdev.hh
 * @date   novembre  9, 2022
 * @brief  Brief description here
 */

#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include "spdk/stdinc.h"

#include "spdk/env.h"
#include "spdk/log.h"
#include "spdk/nvme.h"
#include "spdk/nvme_zns.h"
#include "spdk/string.h"
#include "spdk/vmd.h"
#pragma GCC diagnostic pop

/**
 * @brief Userspace read/write access to NVMe devices
 */
class UserBlkDev {
  spdk_nvme_transport_id g_trid = {};
  bool g_vmd = false;

private:
  struct ubd_sequence {
    struct ns_entry *ns_entry;
    char *buf;
    bool is_write;
    unsigned using_cmb_io;
    int is_completed;
  };

  static void register_ns(struct spdk_nvme_ctrlr *ctrlr,
                          struct spdk_nvme_ns *ns);
  static void io_complete(void *arg, const spdk_nvme_cpl *completion);
  static void reset_zone_complete(void *arg, const spdk_nvme_cpl *completion);
  void reset_zone_and_wait_for_completion(ubd_sequence *sequence);
  static bool probe_cb(void *cb_ctx, const spdk_nvme_transport_id *trid,
                       spdk_nvme_ctrlr_opts *opts);
  static void attach_cb(void *cb_ctx, const spdk_nvme_transport_id *trid,
                        spdk_nvme_ctrlr *ctrlr,
                        const spdk_nvme_ctrlr_opts *opts);
  static void cleanup(void);

public:
  UserBlkDev();

  ~UserBlkDev();

  /**
   * @brief Initialize the controllers and IO queues
   * @detail Must be called before reads/writes
   **/
  int init();

  /** @brief Blocking read to an LBA **/
  int read_blocking(void *buf, off_t start_lba, off_t lba_count);

  /** @brief Blocking write to an LBA **/
  int write_blocking(void *buf, off_t start_lba, off_t lba_count);
};
