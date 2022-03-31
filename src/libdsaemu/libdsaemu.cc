#include "libdsaemu.hh"
#include "nvsl/pmemops.hh"

#include <cstring>
#include <pthread.h>

using namespace dsa;

size_t dsa::queue_head;
size_t dsa::queue_tail;
jobdesc_t dsa::queue[QSIZE];

nvsl::PMemOpsClwb *pmemops;

void *dsa_logic_loop(void*) {
  while (true) {
    if (queue_head != queue_tail) {
      jobdesc_t *current_entry = &queue[queue_tail];
      std::memcpy((void*)current_entry->dst, (void*)current_entry->src,
                  current_entry->bytes);

      if (current_entry->flags & jobdesc_t::flags_t::FLUSH)
        pmemops->flush((void*)current_entry->dst, current_entry->bytes);

      if (current_entry->flags & jobdesc_t::flags_t::DRAIN)
        pmemops->drain();
      
      queue_head++;
    }
  }
}

__attribute__((constructor))
static void libdsaemu_ctor() {
  pmemops = new nvsl::PMemOpsClwb();
  
  std::memset(&queue, 0, sizeof(queue));

  queue_head = queue_tail = 0;

  pthread_t tid;
  pthread_create(&tid, NULL, dsa_logic_loop, NULL);
  pthread_detach(tid);
}

void clear_queue() {
  queue_head = queue_tail = 0;
}

