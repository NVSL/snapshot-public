// Copyright (C) 2019-2020 Terence Kelly.  All rights reserved.
// Hand-over-hand locking example program.  Intended to accompany
// article in USENIX _;login:_ magazine circa late 2020.
// Compile as C99 or C11; for gcc, compile & link with "-pthread".

#include <assert.h>
#include <errno.h>
#include <libpmemobj.h>
#include <libpmemobj/base.h>
#include <libpmemobj/tx.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


POBJ_LAYOUT_BEGIN(hoh);
POBJ_LAYOUT_ROOT(hoh, struct node_t)
POBJ_LAYOUT_ROOT(hoh, struct root_t)
POBJ_LAYOUT_END(hoh)

struct node_t {
  pthread_mutex_t m;
  int data;
  TOID(struct node_t) next;
  char name[32];
};

#define comp_nodes(node_a, node_b) \
  (D_RW(node_a)->data == D_RW(node_b)->data) &&  \
  (D_RO(D_RW(node_a)->next) == D_RO(D_RW(node_b)->data)) &&     \
  (D_RW(node_a)->data == D_RW(node_b)->data) && 

struct root_t {
  TOID(struct node_t) head;
};

#define PMI PTHREAD_MUTEX_INITIALIZER

#define DIE(s) (perror(s), assert(0), 1)
#define PT(f, ...) ((errno = pthread_##f(__VA_ARGS__)) && DIE(#f))

#define LOCK(p) PT(mutex_lock, (&(D_RW(p)->m)))
#define UNLK(p) ((void)PT(mutex_unlock, (&(D_RW(p)->m))), TOID_ASSIGN(p, OID_NULL))

static pthread_mutex_t pm = PMI; // print mutex
#define printf(...)        \
  do {                     \
    PT(mutex_lock, &pm);   \
    printf(__VA_ARGS__);   \
    PT(mutex_unlock, &pm); \
  } while (0)

static TOID(struct node_t) create_dummy_list(PMEMobjpool *pop) {
  TOID(struct node_t) result, current;
  int data_cnt = 0;

  TX_BEGIN(pop) {
    result = TX_NEW(struct node_t);
    current = result;
    memcpy(D_RW(result)->name, "A\0", 2);
    D_RW(current)->data = data_cnt++;
    PT(mutex_init, &D_RW(current)->m, NULL);

    D_RW(current)->next = TX_NEW(struct node_t);
    current = D_RW(current)->next;
    memcpy(D_RW(result)->name, "B\0", 2);
    D_RW(current)->data = data_cnt++;
    PT(mutex_init, &D_RW(current)->m, NULL);

    D_RW(current)->next = TX_NEW(struct node_t);
    current = D_RW(current)->next;
    memcpy(D_RW(result)->name, "C\0", 2);
    D_RW(current)->data = data_cnt++;
    PT(mutex_init, &D_RW(current)->m, NULL);

    D_RW(current)->next = TX_NEW(struct node_t);
    current = D_RW(current)->next;
    memcpy(D_RW(result)->name, "D\0", 2);
    D_RW(current)->data = data_cnt++;
    PT(mutex_init, &D_RW(current)->m, NULL);
    D_RW(current)->next = TX_NEW(struct node_t);

    D_RW(current)->next = TX_NEW(struct node_t);
    current = D_RW(current)->next;
    memcpy(D_RW(result)->name, "E\0", 2);
    D_RW(current)->data = data_cnt++;
    PT(mutex_init, &D_RW(current)->m, NULL);
    D_RW(current)->next = TX_NEW(struct node_t);

    D_RW(current)->next = TX_NEW(struct node_t);
    current = D_RW(current)->next;
    memcpy(D_RW(result)->name, "F\0", 2);
    D_RW(current)->data = data_cnt++;
    PT(mutex_init, &D_RW(current)->m, NULL);
    D_RW(current)->next = TX_NEW(struct node_t);

    current = D_RW(current)->next;
    memcpy(D_RW(result)->name, "G\0", 2);
    D_RW(current)->data = data_cnt++;
    TOID_ASSIGN(D_RW(current)->next, OID_NULL);
    PT(mutex_init, &D_RW(current)->m, NULL);
  } TX_END;

  return result;
}

static void *hoh(void *args) {
  char *id = (char *)(((void**)args)[0]);
  struct root_t *root = (struct root_t *)(((void**)args)[1]);

  TOID (struct node_t) p, n; // "previous" follows "next" down the list
  printf("%s: begin\n", id);
  for (p = root->head, LOCK(p); NULL != D_RO(n = D_RW(p)->next); p = n) {
    TX_BEGIN(pmemobj_pool_by_ptr(root)) {
      // A:  *p locked & might be dummy head
      //     *n not yet locked & can't be head
      LOCK(n);
      // B:  we may remove *n here
      UNLK(p);
      // C:  best place to inspect *n or insert node after *n
      printf("%s: node %s @ %p data %d\n", id, D_RW(n)->name, (void *)D_RW(n), D_RW(n)->data);
      D_RW(n)->data++;
    } TX_ONABORT {
      fprintf(stderr, "ERR: TX aborted\n");
      exit(1);
    } TX_END;
  }
  // D
  sleep(1) && DIE("sleep"); // stall for "convoy" interleaving
  UNLK(p);
  printf("%s: end\n", id);
  return id;
}

#define NTHREADS 4
int main(int argc, char* argv[]) {
  PMEMobjpool *pool = pmemobj_open("/mnt/pmem0/hoh.pool", "hoh");
  if (pool == NULL) {
    DIE("pmemobj_open");
  }
  
  TOID(struct root_t) root = POBJ_ROOT(pool, struct root_t);

  if (TOID_IS_NULL(root))
    DIE("TOID_IS_NULL");

  D_RW(root)->head = create_dummy_list(pool);
  
  pthread_t t[NTHREADS];
  int i;
  void *tr;
  char m1[] = "1st (serial) traversal", m2[] = "2nd (serial) traversal",
       id[NTHREADS][3] = {{"T0"}, {"T1"}, {"T2"}, {"T3"}};

  void *args[2];

  args[0] = (void*)m1;
  args[1] = (void*)D_RW(root);
  hoh((void *)args);

  args[0] = (void*)m2;
  hoh((void *)args);
  printf("\nmain: going multi-threaded:\n\n");
  for (i = 0; i < NTHREADS; i++) {
    void *vals[2];
    vals[0] = (void*)id[i];
    vals[1] = (void*)D_RW(root);
    PT(create, &t[i], NULL, hoh, (void*)vals);
  }
  for (i = 0; i < NTHREADS; i++) {
    PT(join, t[i], &tr);
    printf("main: joined %s\n", (char *)tr);
  }
  printf("\nmain: all threads finished\n");
  return 0;
}
