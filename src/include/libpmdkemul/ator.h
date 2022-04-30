#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *mmf_create(const char *path, size_t bytes);
void *mmf_alloc_root(void *mmf_obj, size_t bytes);
void *mmf_get_root(void *mmf_obj, size_t bytes);
void *mmf_get_or_alloc_root(void *mmf_obj, size_t bytes);
void *mmf_alloc(void *mmf_obj, size_t bytes);
void mmf_free(void *mmf_obj, void *ptr);
void mmf_snapshot(void *mmf_obj);

#ifdef __cplusplus
}
#endif
