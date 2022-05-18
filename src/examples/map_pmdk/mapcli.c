// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2019, Intel Corporation */

#include <assert.h>
#include "ex_common.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <inttypes.h>

#include <libpmemobj.h>

#include "map.h"
#include "map_btree.h"

#define PM_HASHSET_POOL_SIZE (160 * 1024 * 1024)
#define BULK_INSERT_CNT (400000)

POBJ_LAYOUT_BEGIN(map);
POBJ_LAYOUT_ROOT(map, struct root);
POBJ_LAYOUT_END(map);

struct root {
	TOID(struct map) map;
};

static PMEMobjpool *pop;
static struct map_ctx *mapc;
static TOID(struct root) root;
static TOID(struct map) map;

/*
 * str_insert -- hs_insert wrapper which works on strings
 */
static void
str_insert(const char *str)
{
	uint64_t key;
	if (sscanf(str, "%" PRIu64, &key) > 0)
		map_insert(mapc, map, key, OID_NULL);
	else
		fprintf(stderr, "insert: invalid syntax\n");
}

/*
 * str_remove -- hs_remove wrapper which works on strings
 */
static void
str_remove(const char *str)
{
	uint64_t key;
	if (sscanf(str, "%" PRIu64, &key) > 0) {
		int l = map_lookup(mapc, map, key);
		if (l)
			map_remove(mapc, map, key);
		else
			fprintf(stderr, "no such value\n");
	} else
		fprintf(stderr,	"remove: invalid syntax\n");
}

/*
 * str_check -- hs_check wrapper which works on strings
 */
static void
str_check(const char *str)
{
	uint64_t key;
	if (sscanf(str, "%" PRIu64, &key) > 0) {
		int r = map_lookup(mapc, map, key);
		printf("%d\n", r);
	} else {
		fprintf(stderr, "check: invalid syntax\n");
	}
}

/*
 * str_insert_random -- inserts specified (as string) number of random numbers
 */
static void
str_insert_random(const char *str)
{
	uint64_t val;
	if (sscanf(str, "%" PRIu64, &val) > 0)
		for (uint64_t i = 0; i < val; ) {
			uint64_t r = ((uint64_t)rand()) << 32 | rand();
			int ret = map_insert(mapc, map, r, OID_NULL);
			if (ret < 0)
				break;
			if (ret == 0)
				i += 1;
		}
	else
		fprintf(stderr, "random insert: invalid syntax\n");
}


static void
help(void)
{
	printf("h - help\n");
	printf("i $value - insert $value\n");
	printf("r $value - remove $value\n");
	printf("c $value - check $value, returns 0/1\n");
	printf("n $value - insert $value random values\n");
	printf("p - print all values\n");
	printf("d - print debug info\n");
	printf("b [$value] - rebuild $value (default: 1) times\n");
	printf("q - quit\n");
}

static void
unknown_command(const char *str)
{
	fprintf(stderr, "unknown command '%c', use 'h' for help\n", str[0]);
}

static int
hashmap_print(uint64_t key, PMEMoid value, void *arg)
{
	printf("%" PRIu64 " ", key);
	return 0;
}

static void
print_all(void)
{
	if (mapc->ops->count)
		printf("count: %zu\n", map_count(mapc, map));
	map_foreach(mapc, map, hashmap_print, NULL);
	printf("\n");
}

#define INPUT_BUF_LEN 1000
int
main(int argc, char *argv[])
{
	if (argc < 3 || argc > 4) {
		printf("usage: %s "
			"hashmap_tx|hashmap_atomic|hashmap_rp|"
			"ctree|btree|rtree|rbtree|skiplist"
				" file-name [<seed>]\n", argv[0]);
		return 1;
	}

	const struct map_ops *ops = NULL;
	const char *path = argv[2];
	const char *type = argv[1];
	if (strcmp(type, "btree") == 0) {
		ops = MAP_BTREE;
	} else {
		fprintf(stderr, "invalid container type -- '%s'\n", type);
		return 1;
	}

	if (file_exists(path) != 0) {
		pop = pmemobj_create(path, POBJ_LAYOUT_NAME(map),
			PM_HASHSET_POOL_SIZE, CREATE_MODE_RW);

		if (pop == NULL) {
			fprintf(stderr, "failed to create pool: %s\n",
					pmemobj_errormsg());
			return 1;
		}

		mapc = map_ctx_init(ops, pop);
		if (!mapc) {
			pmemobj_close(pop);
			perror("map_ctx_init");
			return 1;
		}

		root = POBJ_ROOT(pop, struct root);

		map_create(mapc, &D_RW(root)->map, NULL);

		map = D_RO(root)->map;
	} else {
		pop = pmemobj_open(path, POBJ_LAYOUT_NAME(map));
		if (pop == NULL) {
			fprintf(stderr, "failed to open pool: %s\n",
					pmemobj_errormsg());
			return 1;
		}

		mapc = map_ctx_init(ops, pop);
		if (!mapc) {
			pmemobj_close(pop);
			perror("map_ctx_init");
			return 1;
		}
		root = POBJ_ROOT(pop, struct root);
		map = D_RO(root)->map;
	}

	char buf[INPUT_BUF_LEN];

	printf("Type 'h' for help\n$ ");

	while (fgets(buf, sizeof(buf), stdin)) {
		if (buf[0] == 0 || buf[0] == '\n')
			continue;

		switch (buf[0]) {
		        case 'k': {
				
				struct timespec ts_start, ts_end;
				assert(TIME_UTC == timespec_get(&ts_start, TIME_UTC));
				for (int i = 0; i < BULK_INSERT_CNT; i++) {
					map_insert(mapc, map, rand(), OID_NULL);
				}
				assert(TIME_UTC == timespec_get(&ts_end, TIME_UTC));

				const uint64_t elapsed_ns
					= (ts_end.tv_sec - ts_start.tv_sec)*1000*1000*1000
					+ (ts_end.tv_nsec - ts_start.tv_nsec);
				const double elapsed_s = elapsed_ns / (double)(1000*1000*1000);
				const double insertion_per_s = (BULK_INSERT_CNT) / elapsed_s;

				printf("Elapsed ns = %lu\n", elapsed_ns);
				printf("Elapsed s = %lf\n", elapsed_s);
				printf("Rate = %lf\n", insertion_per_s);

				break;
		        } 
		        case 'R': {
				double elapsed_s = 0;
				uint64_t elapsed_ns = 0;
				uint64_t *keys = (uint64_t*)malloc(BULK_INSERT_CNT*sizeof(uint64_t));
				for (int i = 0; i < BULK_INSERT_CNT; i++) {
					keys[i] = rand();
					map_insert(mapc, map, keys[i], OID_NULL);
				}

				struct timespec ts_start, ts_end;
				assert(TIME_UTC == timespec_get(&ts_start, TIME_UTC));
				for (int i = 0; i < BULK_INSERT_CNT; i++) {
					map_remove(mapc, map, keys[i]);
				}
				assert(TIME_UTC == timespec_get(&ts_end, TIME_UTC));

				elapsed_ns
					+= (ts_end.tv_sec - ts_start.tv_sec)*1000*1000*1000
					+ (ts_end.tv_nsec - ts_start.tv_nsec);
				elapsed_s += elapsed_ns / (double)(1000*1000*1000);

				free(keys);

				const double insertion_per_s = (BULK_INSERT_CNT) / elapsed_s;
				printf("Elapsed ns = %lu\n", elapsed_ns);
				printf("Elapsed s = %lf\n", elapsed_s);
				printf("Rate = %lf\n", insertion_per_s);

				break;
		        } case 'C': {
				uint64_t *keys = (uint64_t*)malloc(BULK_INSERT_CNT*sizeof(uint64_t));
				for (int i = 0; i < BULK_INSERT_CNT; i++) {
					keys[i] = rand();
					map_insert(mapc, map, keys[i], OID_NULL);
				}

				struct timespec ts_start, ts_end;
				assert(TIME_UTC == timespec_get(&ts_start, TIME_UTC));
				for (int i = 0; i < BULK_INSERT_CNT; i++) {
					map_lookup(mapc, map, keys[i]);
				}
				assert(TIME_UTC == timespec_get(&ts_end, TIME_UTC));

				const uint64_t elapsed_ns
					= (ts_end.tv_sec - ts_start.tv_sec)*1000*1000*1000
					+ (ts_end.tv_nsec - ts_start.tv_nsec);
				const double elapsed_s = elapsed_ns / (double)(1000*1000*1000);
				const double insertion_per_s = (BULK_INSERT_CNT) / elapsed_s;

				printf("Elapsed ns = %lu\n", elapsed_ns);
				printf("Elapsed s = %lf\n", elapsed_s);
				printf("Rate = %lf\n", insertion_per_s);

				break;
		        } case 'i':
				str_insert(buf + 1);
				break;
			case 'r':
				str_remove(buf + 1);
				break;
			case 'c':
				str_check(buf + 1);
				break;
			case 'n':
				str_insert_random(buf + 1);
				break;
			case 'p':
				print_all();
				break;
			case 'q':
				fclose(stdin);
				break;
			case 'h':
				help();
				break;
			default:
				unknown_command(buf);
				break;
		}

		if (isatty(fileno(stdout)))
			printf("$ ");
	}

	map_ctx_free(mapc);
	pmemobj_close(pop);

	return 0;
}
