#ifndef LIBSTOREINST_H
#define LIBSTOREINST_H

#include <sys/types.h>

extern bool startTracking;


extern "C" int snapshot(void *addr, size_t length, int flags);

#endif
