// -*- mode: c++; c-basic-offset: 2; -*-

#pragma once

#include <cstdint>
#include <unistd.h>

void init_dlsyms();
extern void *(*real_memcpy)(void *, const void *, size_t);
extern void *(*real_memmove)(void *, const void *, size_t);
