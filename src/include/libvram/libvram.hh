#pragma once

namespace nvsl {
  namespace libvram {
    void *vram_malloc(size_t bytes);
    void vram_free(void *ptr);
  }
}
