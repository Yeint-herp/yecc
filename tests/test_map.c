#include "map.h"
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define RUN(test)                                                                                                      \
	do {                                                                                                               \
		printf("%-35s", #test);                                                                                        \
		test();                                                                                                        \
		puts("OK");                                                                                                    \
	} while (0)

#define ASSERT(expr) assert(expr)

constexpr size_t SMALL_N = 20;
constexpr size_t FOREACH_N = 100;
constexpr size_t BENCH_N_T = 10000000UL;
constexpr size_t COLLISIONS_N_T = 1000000UL;
constexpr size_t STR_BENCH_N_T = 1000000UL;
constexpr size_t STR_KEY_LEN = 16;

static bool cmp_u32(uint32_t a, uint32_t b) { return a == b; }
static uintptr_t hash_u32(uint32_t x) { return (uintptr_t)x; }

static bool cmp_str(const char *a, const char *b) { return strcmp(a, b) == 0; }
static uintptr_t hash_str(const char *s) {
	uintptr_t h = 146527, c;
	while ((c = (unsigned char)*s++))
		h = (h << 5) ^ c;
	return h;
}

typedef map_of(char *, char *) strmap_t;
typedef map_of(uint32_t, uint32_t) u32map_t;

static double diff_sec(const struct timespec *a, const struct timespec *b) {
	return (b->tv_sec - a->tv_sec) + (b->tv_nsec - a->tv_nsec) * 1e-9;
}

static uint32_t key_inc(uint32_t x) { return x + 1; }
static uint32_t val_dbl(uint32_t x) { return x * 2; }

static char *str_to_upper(const char *s) {
	size_t n = strlen(s), i;
	char *out = malloc(n + 1);
	for (i = 0; i < n; i++)
		out[i] = toupper((unsigned char)s[i]);
	out[n] = '\0';
	return out;
}
static char *str_rev(const char *s) {
	size_t n = strlen(s), i;
	char *out = malloc(n + 1);
	for (i = 0; i < n; i++)
		out[i] = s[n - 1 - i];
	out[n] = '\0';
	return out;
}

static char *u32_to_str(uint32_t x) {
	char buf[12];
	int len = sprintf(buf, "%u", x);
	char *out = malloc(len + 1);
	memcpy(out, buf, len + 1);
	return out;
}

static bool cmp_str_ci(const char *a, const char *b) {
	while (*a && *b) {
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return false;
		a++;
		b++;
	}
	return *a == *b;
}

static uintptr_t hash_str_ci(const char *s) {
	constexpr uintptr_t FNV_offset = 146527;
	constexpr uintptr_t FNV_prime = 1099511628211u;
	uintptr_t h = FNV_offset;
	unsigned char c;
	while ((c = (unsigned char)*s++)) {
		c = (unsigned char)tolower(c);
		h ^= c;
		h *= FNV_prime;
	}
	return h;
}

static char *make_key(uint32_t seed) {
	uint32_t x = seed ^ (uint32_t)getpid();
	char *buf = malloc(STR_KEY_LEN + 1);
	for (size_t i = 0; i < STR_KEY_LEN; i++) {
		x = x * 1664525 + 1013904223;
		buf[i] = 'A' + (x % 26);
	}
	buf[STR_KEY_LEN] = '\0';
	return buf;
}

static void test_init_destroy(void) {
	u32map_t m;
	bool ok = map_init(&m, cmp_u32, hash_u32);
	ASSERT(ok);
	ASSERT(map_size(&m) == 0);
	ASSERT(m.graves == 0);
	ASSERT(map_capacity(&m) >= MAP_DEFAULT_CAPACITY);

	map_destroy(&m);
	ASSERT(m.keys == nullptr);
	ASSERT(m.values == nullptr);
	ASSERT(m.status_bits == nullptr);
	ASSERT(m.capacity == 0 && m.size == 0 && m.graves == 0);
	ASSERT(m.compare == nullptr);
	ASSERT(m.hash == nullptr);
}

static void test_put_get_remove_overwrite(void) {
	u32map_t m;
	ASSERT(map_init(&m, cmp_u32, hash_u32));

	// new insert
	ASSERT(map_put(&m, 42, 4242) == MAP_PUT_NEW);
	ASSERT(map_size(&m) == 1);
	uint32_t *p = map_get(&m, 42);
	ASSERT(p && *p == 4242);

	// overwrite
	ASSERT(map_put(&m, 42, 9999) == MAP_PUT_OVERWRITE);
	p = map_get(&m, 42);
	ASSERT(p && *p == 9999);

	// missing
	ASSERT(map_get(&m, 0xDEAD) == nullptr);
	ASSERT(!map_remove(&m, 0xDEAD));

	// remove existing
	ASSERT(map_remove(&m, 42));
	ASSERT(map_size(&m) == 0);
	ASSERT(map_get(&m, 42) == nullptr);

	map_destroy(&m);
}

static void test_bulk_insert_and_resize(void) {
	u32map_t m;
	ASSERT(map_init(&m, cmp_u32, hash_u32));

	size_t cap0 = map_capacity(&m);
	for (uint32_t i = 0; i < SMALL_N; i++)
		ASSERT(map_put(&m, i, i + 1000) == MAP_PUT_NEW);

	ASSERT(map_capacity(&m) > cap0);
	ASSERT(map_size(&m) == SMALL_N);

	// validate
	for (uint32_t i = 0; i < SMALL_N; i++) {
		uint32_t *v = map_get(&m, i);
		ASSERT(v && *v == i + 1000);
	}

	map_destroy(&m);
}

static void test_get_or_and_contains(void) {
	u32map_t m;
	ASSERT(map_init(&m, cmp_u32, hash_u32));

	// empty
	ASSERT(!map_contains(&m, 5));
	ASSERT(map_get_or(&m, 5, 777) == 777);

	// insert and test
	ASSERT(map_put(&m, 5, 55) == MAP_PUT_NEW);
	ASSERT(map_contains(&m, 5));
	ASSERT(map_get_or(&m, 5, 777) == 55);

	map_destroy(&m);
}

static void test_tombstone_and_grave_reuse(void) {
	u32map_t m;
	ASSERT(map_init(&m, cmp_u32, hash_u32));

	// fill the map with enough keys to trigger a resize
	for (uint32_t i = 0; i < SMALL_N; i++)
		ASSERT(map_put(&m, i, i) == MAP_PUT_NEW);

	size_t cap = map_capacity(&m);
	size_t max_graves = (size_t)(cap * MAP_MAX_GRAVE_RATIO); // maximum number of graves without rehashing

	// remove exactly max_graves keys, without triggering a rehash
	for (uint32_t i = 0; i < max_graves; i++)
		ASSERT(map_remove(&m, i));

	// sanity check: graves match expectation
	size_t graves_before = m.graves;
	ASSERT(graves_before == max_graves);

	// insert a new key that hashes to one of the removed slots (e.g. bucket 0)
	uint32_t colliding = (uint32_t)cap * 2;
	ASSERT(map_put(&m, colliding, 1234) == MAP_PUT_NEW);
	ASSERT(m.graves == graves_before - 1);

	map_destroy(&m);
}

static void test_grave_ratio_triggers_rehash(void) {
	u32map_t m;
	ASSERT(map_init(&m, cmp_u32, hash_u32));

	size_t cap = map_capacity(&m);
	// create enough graves to exceed MAP_MAX_GRAVE_RATIO
	size_t to_remove = (size_t)(cap * MAP_MAX_GRAVE_RATIO) + 1;
	for (uint32_t i = 0; i < (uint32_t)to_remove; i++)
		ASSERT(map_put(&m, i, i) == MAP_PUT_NEW);
	for (uint32_t i = 0; i < (uint32_t)to_remove; i++)
		ASSERT(map_remove(&m, i));

	size_t cap_before = map_capacity(&m);
	// next insert should rehash (same cap) and clear graves
	ASSERT(map_put(&m, 0xBEEF, 42) == MAP_PUT_NEW);
	ASSERT(map_capacity(&m) == cap_before);
	ASSERT(m.graves == 0);

	map_destroy(&m);
}

static void test_clone_and_deep_clone(void) {
	// shallow clone
	u32map_t src;
	ASSERT(map_init(&src, cmp_u32, hash_u32));
	for (uint32_t i = 0; i < FOREACH_N; i++)
		ASSERT(map_put(&src, i, i * 2) == MAP_PUT_NEW);

	u32map_t sh;
	memset(&sh, 0, sizeof sh);
	ASSERT(map_clone(&sh, &src));
	ASSERT(map_size(&sh) == map_size(&src));
	// overwrite src, ensure sh is unaffected
	ASSERT(map_put(&src, 0, 9999) == MAP_PUT_OVERWRITE);
	ASSERT(*map_get(&sh, 0) == 0);

	map_destroy(&src);
	map_destroy(&sh);

	// deep clone with strdup
	strmap_t sm;
	ASSERT(map_init(&sm, cmp_str, hash_str));
	char keybuf[32], valbuf[32];
	for (int i = 0; i < 10; i++) {
		sprintf(keybuf, "key%02d", i);
		sprintf(valbuf, "val%02d", i);
		ASSERT(map_put(&sm, strdup(keybuf), strdup(valbuf)) == MAP_PUT_NEW);
	}
	strmap_t dm;
	memset(&dm, 0, sizeof dm);
	ASSERT(map_clone_deep(&dm, &sm, strdup, strdup));

	map_foreach(&sm, k, v) {
		free(*k);
		free(*v);
	}
	map_destroy(&sm);

	map_foreach(&dm, k, v) {
		ASSERT(strncmp(*k, "key", 3) == 0);
		ASSERT(strncmp(*v, "val", 3) == 0);
		free(*k);
		free(*v);
	}
	map_destroy(&dm);
}

static void test_map_transform_u32(void) {
	u32map_t src, dst;
	ASSERT(map_init(&src, cmp_u32, hash_u32));
	ASSERT(map_init(&dst, cmp_u32, hash_u32));

	for (uint32_t i = 0; i < SMALL_N; i++)
		ASSERT(map_put(&src, i, i + 100) == MAP_PUT_NEW);

	// transform: key=x+1, val=(x+100)*2
	bool ok = map_transform(&dst, &src, key_inc, val_dbl);
	ASSERT(ok);
	ASSERT(map_size(&dst) == SMALL_N);

	// verify
	for (uint32_t i = 0; i < SMALL_N; i++) {
		uint32_t nk = key_inc(i);
		uint32_t nv = val_dbl(i + 100);
		uint32_t *pv = map_get(&dst, nk);
		ASSERT(pv && *pv == nv);
	}

	map_destroy(&src);
	map_destroy(&dst);
}

static void test_map_transform_str(void) {
	strmap_t src, dst;
	ASSERT(map_init(&src, cmp_str, hash_str));
	ASSERT(map_init(&dst, cmp_str, hash_str));

	// fill src with test values
	ASSERT(map_put(&src, strdup("a"), strdup("one")) == MAP_PUT_NEW);
	ASSERT(map_put(&src, strdup("b"), strdup("two")) == MAP_PUT_NEW);
	ASSERT(map_put(&src, strdup("c"), strdup("three")) == MAP_PUT_NEW);

	// transform values into dst
	bool ok = map_transform(&dst, &src, str_to_upper, str_rev);
	ASSERT(ok);
	ASSERT(map_size(&dst) == 3);

	// verify that transformation was correct
	map_foreach(&dst, kk, vv) {
		char *k = *kk, *v = *vv;
		ASSERT(strlen(k) == 1 && isupper((unsigned char)k[0]));
		if (k[0] == 'A')
			ASSERT(strcmp(v, "eno") == 0);
		else if (k[0] == 'B')
			ASSERT(strcmp(v, "owt") == 0);
		else if (k[0] == 'C')
			ASSERT(strcmp(v, "eerht") == 0);
		free(k);
		free(v);
	}

	map_foreach(&src, kk, vv) {
		free(*kk);
		free(*vv);
	}

	map_destroy(&src);
	map_destroy(&dst);
}

static void test_map_transform_u32_to_str(void) {
	u32map_t src;
	strmap_t dst;
	ASSERT(map_init(&src, cmp_u32, hash_u32));
	ASSERT(map_init(&dst, cmp_str, hash_str));

	for (uint32_t i = 0; i < SMALL_N; i++)
		ASSERT(map_put(&src, i, i * 10) == MAP_PUT_NEW);

	// transform both key and value into strings
	bool ok = map_transform(&dst, &src, u32_to_str, u32_to_str);
	ASSERT(ok);
	ASSERT(map_size(&dst) == SMALL_N);

	map_foreach(&dst, kk, vv) {
		uint32_t k = (uint32_t)atoi(*kk);
		uint32_t v = (uint32_t)atoi(*vv);
		// original src must have had that mapping
		ASSERT(map_contains(&src, k));
		ASSERT(*map_get(&src, k) == v);
		free(*kk);
		free(*vv);
	}

	map_destroy(&src);
	map_destroy(&dst);
}

static void test_foreach_iteration(void) { /* bascially already tested by other usage but eh */
	u32map_t m;
	ASSERT(map_init(&m, cmp_u32, hash_u32));
	bool seen[FOREACH_N] = {false};

	for (uint32_t i = 0; i < FOREACH_N; i++)
		ASSERT(map_put(&m, i, i + 1) == MAP_PUT_NEW);

	map_foreach(&m, kk, vv) {
		uint32_t k = *kk, v = *vv;
		ASSERT(v == k + 1);
		seen[k] = true;
	}
	for (size_t i = 0; i < FOREACH_N; i++)
		ASSERT(seen[i]);

	map_destroy(&m);
}

static void test_performance(void) {
	u32map_t m;
	struct timespec t0, t1;
	ASSERT(map_init(&m, cmp_u32, hash_u32));

	printf("\n=== u32map Performance (%lu integer keys) ===\n", BENCH_N_T);

	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (uint32_t i = 0; i < BENCH_N_T; i++)
		ASSERT(map_put(&m, i, i ^ 0xDEADBEEF) == MAP_PUT_NEW);
	clock_gettime(CLOCK_MONOTONIC, &t1);
	printf("insert: %.3f s  ops/s=%.0f\n", diff_sec(&t0, &t1), BENCH_N_T / diff_sec(&t0, &t1));

	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (uint32_t i = 0; i < BENCH_N_T; i++)
		ASSERT(*map_get(&m, i) == (i ^ 0xDEADBEEF));
	clock_gettime(CLOCK_MONOTONIC, &t1);
	printf("lookup: %.3f s  ops/s=%.0f\n", diff_sec(&t0, &t1), BENCH_N_T / diff_sec(&t0, &t1));

	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (uint32_t i = 0; i < BENCH_N_T; i++)
		ASSERT(map_remove(&m, i));
	clock_gettime(CLOCK_MONOTONIC, &t1);
	printf("remove: %.3f s  ops/s=%.0f\n", diff_sec(&t0, &t1), BENCH_N_T / diff_sec(&t0, &t1));

	map_destroy(&m);
}

static void test_collision_stress(void) {
	u32map_t m;
	struct timespec t0, t1;
	ASSERT(map_init(&m, cmp_u32, hash_u32));

	size_t cap = map_capacity(&m);
	uint32_t base = 0x12345678;

	printf("\n=== u32map Collision Stress (%lu forced collisions) ===\n", COLLISIONS_N_T);

	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (uint32_t i = 0; i < COLLISIONS_N_T; i++) {
		uint32_t k = base + i * (uint32_t)cap;
		ASSERT(map_put(&m, k, i) == MAP_PUT_NEW);
	}
	clock_gettime(CLOCK_MONOTONIC, &t1);
	printf("coll-ins: %.3f s  ops/s=%.0f\n", diff_sec(&t0, &t1), COLLISIONS_N_T / diff_sec(&t0, &t1));

	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (uint32_t i = 0; i < COLLISIONS_N_T; i++) {
		uint32_t k = base + i * (uint32_t)cap;
		ASSERT(*map_get(&m, k) == i);
	}
	clock_gettime(CLOCK_MONOTONIC, &t1);
	printf("coll-lkp: %.3f s  ops/s=%.0f\n", diff_sec(&t0, &t1), COLLISIONS_N_T / diff_sec(&t0, &t1));

	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (uint32_t i = 0; i < COLLISIONS_N_T; i++) {
		uint32_t k = base + i * (uint32_t)cap;
		ASSERT(map_remove(&m, k));
	}
	clock_gettime(CLOCK_MONOTONIC, &t1);
	printf("coll-rem: %.3f s  ops/s=%.0f\n", diff_sec(&t0, &t1), COLLISIONS_N_T / diff_sec(&t0, &t1));

	ASSERT(map_size(&m) == 0);
	ASSERT(m.graves == COLLISIONS_N_T);

	map_destroy(&m);
}

static void test_perf_strmap(void) {
	strmap_t m;
	struct timespec t0, t1;

	printf("\n=== Complex strmap Performance (%lu strings of length %ld) ===\n", STR_BENCH_N_T, STR_KEY_LEN);

	ASSERT(map_init(&m, cmp_str_ci, hash_str_ci));

	char **keys = malloc(sizeof(*keys) * STR_BENCH_N_T);
	for (uint32_t i = 0; i < STR_BENCH_N_T; i++) {
		keys[i] = make_key(i);
	}

	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (uint32_t i = 0; i < STR_BENCH_N_T; i++) {
		char *k = strdup(keys[i]);
		char *v = strdup(keys[i] + (STR_KEY_LEN / 2)); // half-string as value
		ASSERT(map_put(&m, k, v) == MAP_PUT_NEW);
	}
	clock_gettime(CLOCK_MONOTONIC, &t1);
	double dt_ins = diff_sec(&t0, &t1);
	printf("str-insert: %.3f s  ops/s=%.0f\n", dt_ins, STR_BENCH_N_T / dt_ins);

	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (uint32_t i = 0; i < STR_BENCH_N_T; i++) {
		char **found = map_get(&m, keys[i]);
		ASSERT(found != nullptr);
		ASSERT(found && strlen(*found) == STR_KEY_LEN - (STR_KEY_LEN / 2));
	}
	clock_gettime(CLOCK_MONOTONIC, &t1);
	double dt_lkp = diff_sec(&t0, &t1);
	printf("str-lookup: %.3f s  ops/s=%.0f\n", dt_lkp, STR_BENCH_N_T / dt_lkp);

	clock_gettime(CLOCK_MONOTONIC, &t0);
	for (uint32_t i = 0; i < STR_BENCH_N_T; i++) {
		ASSERT(map_remove(&m, keys[i]));
	}
	clock_gettime(CLOCK_MONOTONIC, &t1);
	double dt_rem = diff_sec(&t0, &t1);
	printf("str-remove: %.3f s  ops/s=%.0f\n", dt_rem, STR_BENCH_N_T / dt_rem);

	map_destroy(&m);
	for (uint32_t i = 0; i < STR_BENCH_N_T; i++) {
		free(keys[i]);
	}
	free(keys);
}

int main(void) {
	setvbuf(stdout, nullptr, _IONBF, 0); // prevent stdout buffering for easier debugging

	puts("\n=== MAP Functional Tests ===");
	RUN(test_init_destroy);
	RUN(test_put_get_remove_overwrite);
	RUN(test_bulk_insert_and_resize);
	RUN(test_get_or_and_contains);
	RUN(test_tombstone_and_grave_reuse);
	RUN(test_grave_ratio_triggers_rehash);
	RUN(test_clone_and_deep_clone);
	RUN(test_map_transform_u32);
	RUN(test_map_transform_str);
	RUN(test_map_transform_u32_to_str);
	RUN(test_foreach_iteration);

	puts("\n=== MAP Performance Tests ===");
	test_performance();
	test_collision_stress();
	test_perf_strmap();

	puts("\nAll tests passed successfully!");
	return 0;
}
