// -*- mode: c++; c-basic-offset: 2; -*-

/**
 * @file   libpmdkemul.cc
 * @date   avril 28, 2022
 * @brief  Brief description here
 */

#include "libpmdkemul/libpmemobj.h"
#include <cstdlib>
#include <cstring>

PMEMobjpool *pmemobj_open(const char *path, const char *layout) {
  (void)layout;

  struct stat st;
  if (-1 == stat(path, &st)) {
    return NULL;
  }

  const off_t size = st.st_size;

  PMEMobjpool *pool = (PMEMobjpool *)malloc(sizeof(PMEMobjpool));
  pool->root = NULL;
  pool->addr = mmf_create(path, size);

  return pool;
}

PMEMobjpool *pmemobj_create(const char *path, const char *layout,
                            size_t poolsize, mode_t mode) {
  (void)layout;
  (void)mode;

  PMEMobjpool *pool = (PMEMobjpool *)malloc(sizeof(PMEMobjpool));
  pool->root = NULL;
  pool->addr = mmf_create(path, poolsize);

  return pool;
}

PMEMoid pmemobj_root(PMEMobjpool *pop, size_t size) {
  PMEMoid result;

  (void)size;

  result.pool_uuid_lo = 1;
  result.off = (size_t)pop->root;

  return result;
}

void pmemobj_close(PMEMobjpool *pop) {
  lpo_assert(0);
}

int pmemobj_check(const char *path, const char *layout) {
  lpo_assert(0);
}

void pmemobj_set_user_data(PMEMobjpool *pop, void *data) {
  lpo_assert(0);
}

void *pmemobj_get_user_data(PMEMobjpool *pop) {
  lpo_assert(0);
}

void *pmemobj_direct(const PMEMoid oid) {
  return (void *)oid.off;
}

PMEMoid pmemobj_tx_alloc(size_t size, uint64_t type_num) {
  PMEMoid res{};

  res.pool_uuid_lo = 1;
  res.off = (uint64_t)mmf_alloc(tx_tls_obj.mmf_obj, size);

  return res;
}

PMEMoid pmemobj_tx_zalloc(size_t size, uint64_t type_num) {
  PMEMoid oid = pmemobj_tx_alloc(size, type_num);
  memset((void *)oid.off, 0, size);
  return oid;
}

PMEMoid pmemobj_tx_xalloc(size_t size, uint64_t type_num, uint64_t flags) {
  lpo_assert(0);
}

PMEMoid pmemobj_tx_realloc(PMEMoid oid, size_t size, uint64_t type_num) {
  lpo_assert(0);
}

PMEMoid pmemobj_tx_zrealloc(PMEMoid oid, size_t size, uint64_t type_num) {
  lpo_assert(0);
}

PMEMoid pmemobj_tx_strdup(const char *s, uint64_t type_num) {
  lpo_assert(0);
}

PMEMoid pmemobj_tx_wcsdup(const wchar_t *s, uint64_t type_num) {
  lpo_assert(0);
}

int pmemobj_tx_free(PMEMoid oid) {
  mmf_free(tx_tls_obj.mmf_obj, (void *)oid.off);
  return 0;
}

int pmemobj_tx_xfree(PMEMoid oid, uint64_t flags) {
  lpo_assert(0);
}

__thread struct tx_tls tx_tls_obj;
