#pragma once

#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "libpmdkemul/ator.h"

#define POBJ_ROOT_TYPE_NUM 0
#define _toid_struct
#define _toid_union
#define _toid_enum
#define _POBJ_LAYOUT_REF(name) (sizeof(_pobj_layout_##name##_ref))

#define TX_ADD_FIELD_DIRECT(discard0, discard1)
#define TX_ADD(discard0)
#define TX_ADD_DIRECT(discard0)

#define lpo_assert(cond)                                           \
  if (!(cond)) {                                                   \
    fprintf(stderr, "%s:%d Assertion failed", __FILE__, __LINE__); \
    exit(1);                                                       \
  }

#define TX_BEGIN(pool)               \
  if (1) {                           \
    tx_tls_obj.mmf_obj = pool->addr; \
    tx_tls_obj.level++;

#define TX_ONABORT \
  }                \
  else if (0) {

// TODO Do an actual call to snapshot
#define TX_END                        \
  }                                   \
  tx_tls_obj.level--;                 \
  if (tx_tls_obj.level == 0) {        \
    mmf_snapshot(tx_tls_obj.mmf_obj); \
    tx_tls_obj.mmf_obj = NULL;        \
  }

#define TX_FREE(o) pmemobj_tx_free(o.oid)

#define TOID_EQUALS(lhs, rhs)        \
  ((lhs).oid.off == (rhs).oid.off && \
   (lhs).oid.pool_uuid_lo == (rhs).oid.pool_uuid_lo)

#define TOID_DECLARE(t, i)                            \
  typedef uint8_t _toid_##t##_toid_type_num[(i) + 1]; \
  TOID(t) {                                           \
    PMEMoid oid;                                      \
    t *_type;                                         \
    _toid_##t##_toid_type_num *_type_num;             \
  }

#define D_RW(o)                                        \
  ({                                                   \
    __typeof__(o) _o;                                  \
    _o._type = NULL;                                   \
    (void)_o;                                          \
    (__typeof__(*(o)._type) *)pmemobj_direct((o).oid); \
  })
#define D_RO(o) ((const __typeof__(*(o)._type) *)pmemobj_direct((o).oid))

#define TOID_IS_NULL(o) ((o).oid.off == 0)

#define POBJ_LAYOUT_BEGIN(discard0)
#define POBJ_LAYOUT_END(discard0)
#define POBJ_LAYOUT_TOID(name, t) TOID_DECLARE(t, (__COUNTER__ + 1));

#define TOID(t) union _toid_##t##_toid
#define TOID_TYPE_NUM(discard0) (1)

#define POBJ_ROOT(pool, t)                                                  \
  (TOID(t)) {                                                               \
    (PMEMoid) { 1, (size_t)(mmf_get_or_alloc_root(pool->addr, sizeof(t))) } \
  }

#define PMEMOBJ_MIN_POOL (16 * 1024 * 1024)

struct tx_tls {
  size_t level;
  void *mmf_obj;
};

typedef struct PMEMoid_t {
  uint64_t pool_uuid_lo;
  uint64_t off;
} PMEMoid;

typedef struct _TOID {
  PMEMoid oid;
} TOID_t;

typedef struct PMEMobjpool_t {
  void *root;
  void *addr;
} PMEMobjpool;

// typedef int mode_t

#ifdef __cplusplus
extern "C" {
#endif
PMEMobjpool *pmemobj_open(const char *path, const char *layout);

PMEMobjpool *pmemobj_create(const char *path, const char *layout,
                            size_t poolsize, mode_t mode);

PMEMoid pmemobj_root(PMEMobjpool *pop, size_t size);

void pmemobj_close(PMEMobjpool *pop);

int pmemobj_check(const char *path, const char *layout);

void pmemobj_set_user_data(PMEMobjpool *pop, void *data);

void *pmemobj_get_user_data(PMEMobjpool *pop);

void *pmemobj_direct(const PMEMoid oid);

PMEMoid pmemobj_tx_alloc(size_t size, uint64_t type_num);

PMEMoid pmemobj_tx_zalloc(size_t size, uint64_t type_num);

PMEMoid pmemobj_tx_xalloc(size_t size, uint64_t type_num, uint64_t flags);

PMEMoid pmemobj_tx_realloc(PMEMoid oid, size_t size, uint64_t type_num);

PMEMoid pmemobj_tx_zrealloc(PMEMoid oid, size_t size, uint64_t type_num);

PMEMoid pmemobj_tx_strdup(const char *s, uint64_t type_num);

PMEMoid pmemobj_tx_wcsdup(const wchar_t *s, uint64_t type_num);

int pmemobj_tx_free(PMEMoid oid);

int pmemobj_tx_xfree(PMEMoid oid, uint64_t flags);

#ifdef __cplusplus
}
#endif

static const PMEMoid OID_NULL = {0, 0};
extern __thread struct tx_tls tx_tls_obj;
