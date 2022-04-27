#pragma once

namespace nvsl {
  namespace libvram {
    void *malloc(size_t bytes);
    void free(void *ptr);
  } // namespace libvram
} // namespace nvsl

void ctor_libvram();
