// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   blkdev.cc
 * @date   novembre  9, 2022
 * @brief  Brief description here
 */

/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright (c) Intel Corporation.
 *   All rights reserved.
 */

#include <functional>
#include <string>

#include "nvsl/common.hh"
#include "nvsl/error.hh"

#include "spdk/stdinc.h"

#include "spdk/env.h"
#include "spdk/log.h"
#include "spdk/nvme.h"
#include "spdk/nvme_zns.h"
#include "spdk/string.h"
#include "spdk/vmd.h"

static TAILQ_HEAD(, ctrlr_entry)
    g_controllers = TAILQ_HEAD_INITIALIZER(g_controllers);
static TAILQ_HEAD(,
                  ns_entry) g_namespaces = TAILQ_HEAD_INITIALIZER(g_namespaces);

struct ns_entry {
  struct spdk_nvme_ctrlr *ctrlr;
  struct spdk_nvme_ns *ns;
  TAILQ_ENTRY(ns_entry) link;
  struct spdk_nvme_qpair *qpair;
};

struct ctrlr_entry {
  struct spdk_nvme_ctrlr *ctrlr;
  TAILQ_ENTRY(ctrlr_entry) link;
  char name[1024];
};

class UserBlkDev {
  spdk_nvme_transport_id g_trid = {};

  bool g_vmd = false;

  static void register_ns(struct spdk_nvme_ctrlr *ctrlr,
                          struct spdk_nvme_ns *ns) {
    struct ns_entry *entry;

    if (!spdk_nvme_ns_is_active(ns)) {
      return;
    }

    entry = new ns_entry;
    if (entry == NULL) {
      perror("ns_entry malloc");
      exit(1);
    }

    entry->ctrlr = ctrlr;
    entry->ns = ns;
    TAILQ_INSERT_TAIL(&g_namespaces, entry, link);

    DBGH(2) << "Namespace ID: " << spdk_nvme_ns_get_id(ns)
            << " size: " << spdk_nvme_ns_get_size(ns) / 1000000000 << "\n";
  }

  struct ubd_sequence {
    struct ns_entry *ns_entry;
    char *buf;
    bool is_write;
    unsigned using_cmb_io;
    int is_completed;
  };

  static void io_complete(void *arg, const struct spdk_nvme_cpl *completion) {
    auto sequence = (ubd_sequence *)arg;

    /* Assume the I/O was successful */
    sequence->is_completed = 1;
    /* See if an error occurred. If so, display information
     * about it, and set completion value so that I/O
     * caller is aware that an error occurred.
     */
    if (spdk_nvme_cpl_is_error(completion)) {
      spdk_nvme_qpair_print_completion(sequence->ns_entry->qpair,
                                       (struct spdk_nvme_cpl *)completion);
      DBGE << "I/O error status: "
           << std::string(spdk_nvme_cpl_get_status_string(&completion->status))
           << "\n";
      DBGE << "Read I/O failed, aborting run\n";
      sequence->is_completed = 2;
    }
  }

  static void reset_zone_complete(void *arg,
                                  const struct spdk_nvme_cpl *completion) {
    auto sequence = (ubd_sequence *)arg;

    /* Assume the I/O was successful */
    sequence->is_completed = 1;
    /* See if an error occurred. If so, display information
     * about it, and set completion value so that I/O
     * caller is aware that an error occurred.
     */
    if (spdk_nvme_cpl_is_error(completion)) {
      spdk_nvme_qpair_print_completion(sequence->ns_entry->qpair,
                                       (struct spdk_nvme_cpl *)completion);
      DBGE << "I/O error status: "
           << spdk_nvme_cpl_get_status_string(&completion->status) << "\n";
      DBGE << "Reset zone I/O failed, aborting run\n";
      sequence->is_completed = 2;
      exit(1);
    }
  }

  void reset_zone_and_wait_for_completion(struct ubd_sequence *sequence) {
    if (spdk_nvme_zns_reset_zone(sequence->ns_entry->ns,
                                 sequence->ns_entry->qpair,
                                 0,     /* starting LBA of the zone to reset */
                                 false, /* don't reset all zones */
                                 reset_zone_complete, sequence)) {
      DBGH(3) << "Starting reset zone I/O failed\n";
      exit(1);
    }
    while (!sequence->is_completed) {
      spdk_nvme_qpair_process_completions(sequence->ns_entry->qpair, 0);
    }
    sequence->is_completed = 0;
  }

  static bool probe_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid,
                       struct spdk_nvme_ctrlr_opts *opts) {
    DBGH(4) << "Attaching to " << trid->traddr << "\n";

    return true;
  }

  static void attach_cb(void *cb_ctx, const struct spdk_nvme_transport_id *trid,
                        struct spdk_nvme_ctrlr *ctrlr,
                        const struct spdk_nvme_ctrlr_opts *opts) {
    int nsid;
    struct ctrlr_entry *entry;
    struct spdk_nvme_ns *ns;
    const struct spdk_nvme_ctrlr_data *cdata;

    entry = new ctrlr_entry;
    if (entry == NULL) {
      perror("ctrlr_entry malloc");
      exit(1);
    }

    DBGH(4) << "Attached to " << trid->traddr << "\n";

    /*
     * spdk_nvme_ctrlr is the logical abstraction in SPDK for an NVMe
     *  controller.  During initialization, the IDENTIFY data for the
     *  controller is read using an NVMe admin command, and that data
     *  can be retrieved using spdk_nvme_ctrlr_get_data() to get
     *  detailed information on the controller.  Refer to the NVMe
     *  specification for more details on IDENTIFY for NVMe controllers.
     */
    cdata = spdk_nvme_ctrlr_get_data(ctrlr);

    snprintf(entry->name, sizeof(entry->name), "%-20.20s (%-20.20s)", cdata->mn,
             cdata->sn);

    entry->ctrlr = ctrlr;
    TAILQ_INSERT_TAIL(&g_controllers, entry, link);

    /*
     * Each controller has one or more namespaces.  An NVMe namespace is
     * basically equivalent to a SCSI LUN.  The controller's IDENTIFY data tells
     * us how many namespaces exist on the controller.  For Intel(R) P3X00
     * controllers, it will just be one namespace.
     *
     * Note that in NVMe, namespace IDs start at 1, not 0.
     */
    for (nsid = spdk_nvme_ctrlr_get_first_active_ns(ctrlr); nsid != 0;
         nsid = spdk_nvme_ctrlr_get_next_active_ns(ctrlr, nsid)) {
      ns = spdk_nvme_ctrlr_get_ns(ctrlr, nsid);
      if (ns == NULL) {
        continue;
      }
      register_ns(ctrlr, ns);
    }
  }

  static void cleanup(void) {
    struct ns_entry *ns_entry, *tmp_ns_entry;
    struct ctrlr_entry *ctrlr_entry, *tmp_ctrlr_entry;
    struct spdk_nvme_detach_ctx *detach_ctx = NULL;

    TAILQ_FOREACH_SAFE(ns_entry, &g_namespaces, link, tmp_ns_entry) {
      TAILQ_REMOVE(&g_namespaces, ns_entry, link);
      free(ns_entry);
    }

    TAILQ_FOREACH_SAFE(ctrlr_entry, &g_controllers, link, tmp_ctrlr_entry) {
      TAILQ_REMOVE(&g_controllers, ctrlr_entry, link);
      spdk_nvme_detach_async(ctrlr_entry->ctrlr, &detach_ctx);
      free(ctrlr_entry);
    }

    if (detach_ctx) {
      spdk_nvme_detach_poll(detach_ctx);
    }
  }

public:
  UserBlkDev() {}

  ~UserBlkDev() {
    cleanup();
    if (g_vmd) {
      spdk_vmd_fini();
    }
  };

  /** @brief Initialize the controllers and IO queues **/
  int init() {
    int rc;
    struct spdk_env_opts opts;

    spdk_env_opts_init(&opts);

    spdk_nvme_trid_populate_transport(&g_trid, SPDK_NVME_TRANSPORT_PCIE);
    snprintf(g_trid.subnqn, sizeof(g_trid.subnqn), "%s",
             SPDK_NVMF_DISCOVERY_NQN);

    opts.name = "libcxlfs";
    if (spdk_env_init(&opts) < 0) {
      DBGE << "Unable to intialize SPDK env\n";
      return 1;
    }

    DBGH(2) << "Initializing NVMe Controllers\n";

    if (g_vmd && spdk_vmd_init()) {
      DBGE << "Failed to initialize VMD. Some NVMe devices can be "
              "unavailable.\n";
    }

    rc = spdk_nvme_probe(&g_trid, NULL, probe_cb, attach_cb, NULL);
    if (rc != 0) {
      DBGE << "spdk_nvme_probe() failed\n";
      rc = 1;
      goto exit;
    }

    if (TAILQ_EMPTY(&g_controllers)) {
      DBGE << "no NVMe controllers found\n";
      rc = 1;
      goto exit;
    }

    struct ns_entry *ns_entry;

    /* Allocate IO Q pairs */
    TAILQ_FOREACH(ns_entry, &g_namespaces, link) {
      ns_entry->qpair =
          spdk_nvme_ctrlr_alloc_io_qpair(ns_entry->ctrlr, NULL, 0);

      if (ns_entry->qpair == NULL) {
        DBGE << "spdk_nvme_ctrlr_alloc_io_qpair() failed\n";
        return -1;
      }
    }

  exit:
    cleanup();
    spdk_env_fini();
    return rc;
  }

  /** @brief Blocking read to an LBA **/
  int read_blocking(void *buf, off_t start_lba, off_t lba_count) {
    struct ns_entry *ns_entry = TAILQ_FIRST(&g_namespaces);
    struct ubd_sequence sequence = {0};
    int rc;

    sequence.using_cmb_io = 1;
    sequence.buf = (char *)spdk_zmalloc(
        0x1000, 0x1000, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);

    if (sequence.buf == NULL) {
      DBGE << "Write buffer allocation failed\n";
      return -1;
    }

    sequence.is_completed = 0;
    sequence.ns_entry = ns_entry;

    /* Check for ZNS */
    if (spdk_nvme_ns_get_csi(ns_entry->ns) == SPDK_NVME_CSI_ZNS) {
      reset_zone_and_wait_for_completion(&sequence);
    }

    rc = spdk_nvme_ns_cmd_read(ns_entry->ns, ns_entry->qpair, sequence.buf,
                               0, /* LBA start */
                               1, /* number of LBAs */
                               io_complete, &sequence, 0);

    if (rc == 0) {
      while (not sequence.is_completed)
        ;
    }

    memcpy(buf, sequence.buf, 0x1000);

    spdk_free(sequence.buf);

    return (sequence.is_completed == 1) ? 0 : -1;
  }

  /** @brief Blocking write to an LBA **/
  int write_blocking(void *buf, off_t start_lba, off_t lba_count) {
    struct ns_entry *ns_entry = TAILQ_FIRST(&g_namespaces);
    struct ubd_sequence sequence = {0};
    int rc;
    size_t sz;

    sequence.using_cmb_io = 1;
    sequence.buf = (char *)spdk_nvme_ctrlr_map_cmb(ns_entry->ctrlr, &sz);
    if (sequence.buf == NULL || sz < 0x1000) {
      sequence.using_cmb_io = 0;
      sequence.buf = (char *)spdk_zmalloc(
          0x1000, 0x1000, NULL, SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
    }
    if (sequence.buf == NULL) {
      DBGE << "Write buffer allocation failed\n";
      return -1;
    }

    memcpy(sequence.buf, buf, 0x1000);

    sequence.is_completed = 0;
    sequence.ns_entry = ns_entry;
    sequence.is_write = true;

    /* Check for ZNS */
    if (spdk_nvme_ns_get_csi(ns_entry->ns) == SPDK_NVME_CSI_ZNS) {
      reset_zone_and_wait_for_completion(&sequence);
    }

    rc = spdk_nvme_ns_cmd_write(ns_entry->ns, ns_entry->qpair, sequence.buf,
                                0, /* LBA start */
                                1, /* number of LBAs */
                                io_complete, &sequence, 0);

    if (rc == 0) {
      while (not sequence.is_completed)
        ;
    }

    if (sequence.using_cmb_io) {
      spdk_nvme_ctrlr_unmap_cmb(sequence.ns_entry->ctrlr);
    } else {
      spdk_free(sequence.buf);
    }

    return (sequence.is_completed == 1) ? 0 : -1;
  }
};
