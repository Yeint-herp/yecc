#include "map.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

static bool cmp_uint32(uint32_t a, uint32_t b) {
	if (a == b)
		return true;
	return false;
}

static uintptr_t hash_uint32(uint32_t x) { return (uintptr_t)x; }

static double timespec_diff(struct timespec *start, struct timespec *end) {
	return (end->tv_sec - start->tv_sec) + (end->tv_nsec - start->tv_nsec) * 1e-9;
}

int main(void) {
	typedef map_of(uint32_t, uint32_t) map_t;
	map_t m;

	printf("Testing map_init... ");
	bool ok = map_init(&m, cmp_uint32, hash_uint32);
	assert(ok);
	assert(map_size(&m) == 0);
	assert(m.graves == 0);
	size_t init_cap = map_capacity(&m);
	assert(init_cap >= MAP_DEFAULT_CAPACITY);
	printf("OK\n");

	printf("Testing single insert... ");
	int r = map_put(&m, 42, 4242);
	assert(r == 0);
	assert(map_size(&m) == 1);
	assert(m.graves == 0);
	uint32_t *pv = map_get(&m, 42);
	assert(pv && *pv == 4242);
	printf("OK\n");

	printf("Testing overwrite... ");
	r = map_put(&m, 42, 4243);
	assert(r == 1);
	assert(map_size(&m) == 1);
	pv = map_get(&m, 42);
	assert(pv && *pv == 4243);
	printf("OK\n");

	printf("Testing bulk insert and resize... ");
	size_t old_cap = map_capacity(&m);
	const uint32_t N = 20;
	for (uint32_t i = 0; i < N; ++i) {
		r = map_put(&m, i, i + 1000);
		assert(r == 0);
		assert(r != 2);
		assert(map_size(&m) == 1 + (i + 1));
	}
	size_t new_cap = map_capacity(&m);
	assert(new_cap > old_cap);
	for (uint32_t i = 0; i < N; ++i) {
		pv = map_get(&m, i);
		assert(pv && *pv == i + 1000);
	}
	pv = map_get(&m, 42);
	assert(pv && *pv == 4243);
	printf("OK\n");

	printf("Testing get missing key... ");
	pv = map_get(&m, 9999);
	assert(pv == nullptr);
	printf("OK\n");

	printf("Testing remove non-existing... ");
	bool b = map_remove(&m, 9999);
	assert(b == false);
	printf("OK\n");

	printf("Testing remove existing... ");
	b = map_remove(&m, 42);
	assert(b == true);
	assert(map_size(&m) == (size_t)(1 + N - 1));
	assert(m.graves == 1);
	pv = map_get(&m, 42);
	assert(pv == nullptr);
	printf("OK\n");

	printf("Testing tombstone reuse... ");
	size_t cap = map_capacity(&m);
	uint32_t newkey = 42 + (uint32_t)cap;
	r = map_put(&m, newkey, 4244);
	assert(r == 0);
	assert(map_size(&m) == (size_t)(1 + N));
	assert(m.graves == 0);
	pv = map_get(&m, newkey);
	assert(pv && *pv == 4244);
	printf("OK\n");

	printf("Testing clear... ");
	map_clear(&m);
	assert(map_size(&m) == 0);
	assert(m.graves == 0);
	assert(map_get(&m, 0) == nullptr);
	assert(map_get(&m, newkey) == nullptr);
	printf("OK\n");

	printf("Testing destroy... ");
	map_destroy(&m);
	assert(m.keys == nullptr);
	assert(m.values == nullptr);
	assert(m.status_bits == nullptr);
	assert(m.capacity == 0);
	assert(m.size == 0);
	assert(m.graves == 0);
	assert(m.compare == nullptr);
	assert(m.hash == nullptr);
	printf("OK\n");

	printf("Testing foreach iteration... ");
	map_init(&m, cmp_uint32, hash_uint32);

	const uint32_t M = 100;
	for (uint32_t i = 0; i < M; ++i)
		map_put(&m, i, i * 10);

	bool seen[M];
	memset(seen, 0, sizeof seen);

	uint32_t k, v;
	map_foreach(&m, k, v) {
		assert(k < M);
		assert(v == k * 10);
		seen[k] = true;
	}

	for (uint32_t i = 0; i < M; ++i)
		assert(seen[i]);

	map_destroy(&m);
	printf("OK\n");

	printf("All tests passed!\n");

	const size_t BENCH_N = 10000000UL;
	printf("Benchmark: %zu inserts, lookups, removes\n", BENCH_N);

	ok = map_init(&m, cmp_uint32, hash_uint32);
	assert(ok);

	struct timespec t0, t1;
	double dt_insert, dt_lookup, dt_remove;

	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (uint32_t i = 0; i < BENCH_N; ++i) {
		int r = map_put(&m, i, i ^ 0xDEADBEEF);
		assert(r == 0);
	}
	clock_gettime(CLOCK_MONOTONIC, &t1);
	dt_insert = timespec_diff(&t0, &t1);
	printf("Insert: %.3f s -> %.0f ops/s\n", dt_insert, BENCH_N / dt_insert);

	assert(map_size(&m) == BENCH_N);

	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (uint32_t i = 0; i < BENCH_N; ++i) {
		uint32_t *pv = map_get(&m, i);
		assert(pv && *pv == (i ^ 0xDEADBEEF));
	}
	clock_gettime(CLOCK_MONOTONIC, &t1);
	dt_lookup = timespec_diff(&t0, &t1);
	printf("Lookup: %.3f s -> %.0f ops/s\n", dt_lookup, BENCH_N / dt_lookup);

	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (uint32_t i = 0; i < BENCH_N; ++i) {
		bool b = map_remove(&m, i);
		assert(b);
	}
	clock_gettime(CLOCK_MONOTONIC, &t1);
	dt_remove = timespec_diff(&t0, &t1);
	printf("Remove: %.3f s -> %.0f ops/s\n", dt_remove, BENCH_N / dt_remove);

	assert(map_size(&m) == 0);

	map_destroy(&m);
	printf("Benchmark complete.\n");

	const uint32_t COLLISION_COUNT = 10000000UL;
	printf("\nCollision stress test (%u colliding keys)...\n", COLLISION_COUNT);

	ok = map_init(&m, cmp_uint32, hash_uint32);
	assert(ok);

	cap = map_capacity(&m);
	uint32_t base_key = 0x12345678;

	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (uint32_t i = 0; i < COLLISION_COUNT; ++i) {
		uint32_t key = base_key + i * cap;
		uint32_t val = i * 10 + 1;
		int r = map_put(&m, key, val);
		assert(r == 0);
	}
	clock_gettime(CLOCK_MONOTONIC, &t1);
	dt_insert = timespec_diff(&t0, &t1);
	printf("  Insert: %.6f s -> %.0f ops/s\n", dt_insert, COLLISION_COUNT / dt_insert);

	assert(map_size(&m) == COLLISION_COUNT);

	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (uint32_t i = 0; i < COLLISION_COUNT; ++i) {
		uint32_t key = base_key + i * cap;
		uint32_t expected = i * 10 + 1;
		uint32_t *pv = map_get(&m, key);
		assert(pv && *pv == expected);
	}
	clock_gettime(CLOCK_MONOTONIC, &t1);
	dt_lookup = timespec_diff(&t0, &t1);
	printf("  Lookup: %.6f s -> %.0f ops/s\n", dt_lookup, COLLISION_COUNT / dt_lookup);

	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (uint32_t i = 0; i < COLLISION_COUNT; ++i) {
		uint32_t key = base_key + i * cap;
		bool b = map_remove(&m, key);
		assert(b);
	}
	clock_gettime(CLOCK_MONOTONIC, &t1);
	dt_remove = timespec_diff(&t0, &t1);
	printf("  Remove: %.6f s -> %.0f ops/s\n", dt_remove, COLLISION_COUNT / dt_remove);

	assert(map_size(&m) == 0);
	assert(m.graves == COLLISION_COUNT);

	map_destroy(&m);
	printf("Collision stress test complete.\n");

	return 0;
}
