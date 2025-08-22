#include "vector.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define ASSERT(expr) assert(expr)
#define RUN(test)                                                                                                      \
	do {                                                                                                               \
		printf("%-35s", #test);                                                                                        \
		test();                                                                                                        \
		puts("OK");                                                                                                    \
	} while (0)

static const size_t VECTOR_N = 20;

static void test_vector_init_destroy(void) {
	vector_of(int) v = {};

	ASSERT(v.data == nullptr);
	ASSERT(v.size == 0);
	ASSERT(v.capacity == 0);

	vector_destroy(&v);

	ASSERT(v.data == nullptr);
	ASSERT(v.size == 0);
	ASSERT(v.capacity == 0);
}

static void test_vector_push_pop_back(void) {
	vector_of(int) v = {};

	// push 0..VECTOR_N-1, then pop back in reverse order
	for (int i = 0; i < (int)VECTOR_N; i++) {
		ASSERT(vector_push(&v, i));
		int *pb = vector_back(&v);
		ASSERT(pb && *pb == i);
	}
	ASSERT(vector_size(&v) == VECTOR_N);

	for (int i = VECTOR_N - 1; i >= 0; i--) {
		int x = vector_pop(&v);
		ASSERT(x == i);
	}
	ASSERT(vector_empty(&v));

	vector_destroy(&v);
}

static void test_vector_clear_and_empty_size(void) {
	vector_of(int) v = {};

	for (int i = 0; i < (int)VECTOR_N; i++)
		ASSERT(vector_push(&v, i));
	ASSERT(!vector_empty(&v));
	ASSERT(vector_size(&v) == VECTOR_N);

	vector_clear(&v);
	ASSERT(vector_empty(&v));
	ASSERT(vector_size(&v) == 0);

	vector_destroy(&v);
}

static void test_vector_reserve_and_get(void) {
	vector_of(int) v = {};
	size_t want = 10;

	ASSERT(vector_reserve(&v, want));
	ASSERT(v.capacity >= want);

	for (int i = 0; i < (int)want; i++)
		ASSERT(vector_push(&v, i * 2));
	ASSERT(vector_size(&v) == want);

	for (size_t i = 0; i < want; i++)
		ASSERT(vector_get(&v, i) == (int)(i * 2));

	vector_destroy(&v);
}

static void test_vector_insert_and_erase(void) {
	vector_of(int) v = {};

	// start with [0,1,2,3,4]
	for (int i = 0; i < 5; i++)
		ASSERT(vector_push(&v, i));

	// insert at front -> [100,0,1,2,3,4]
	ASSERT(vector_insert(&v, 0, 100));
	ASSERT(vector_get(&v, 0) == 100);
	ASSERT(vector_size(&v) == 6);

	// insert in middle -> [100,0,1,200,2,3,4]
	ASSERT(vector_insert(&v, 3, 200));
	ASSERT(vector_get(&v, 3) == 200);
	ASSERT(vector_size(&v) == 7);

	// insert at end -> [...,4,300]
	size_t idx_end = vector_size(&v);
	ASSERT(vector_insert(&v, idx_end, 300));
	ASSERT(vector_get(&v, vector_size(&v) - 1) == 300);
	ASSERT(vector_size(&v) == 8);

	// now v == [100,0,1,200,2,3,4,300]

	// erase front -> [0,1,200,2,3,4,300]
	vector_erase(&v, 0);
	ASSERT(vector_get(&v, 0) == 0);
	ASSERT(vector_size(&v) == 7);

	// erase middle (index 2) -> [0,1,2,3,4,300]
	vector_erase(&v, 2);
	ASSERT(vector_get(&v, 2) == 2);
	ASSERT(vector_size(&v) == 6);

	// erase end -> [0,1,2,3,4]
	vector_erase(&v, vector_size(&v) - 1);
	ASSERT(vector_size(&v) == 5);

	// final check
	int expected[] = {0, 1, 2, 3, 4};
	for (size_t i = 0; i < vector_size(&v); i++)
		ASSERT(vector_get(&v, i) == expected[i]);

	vector_destroy(&v);
}

int main(void) {
	puts("\n=== VECTOR Functional Tests ===");
	RUN(test_vector_init_destroy);
	RUN(test_vector_push_pop_back);
	RUN(test_vector_clear_and_empty_size);
	RUN(test_vector_reserve_and_get);
	RUN(test_vector_insert_and_erase);

	printf("\nAll vector tests passed successfully!\n");
	return 0;
}
