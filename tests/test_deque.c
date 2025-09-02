#include "base/deque.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define RUN(test)                                                                                                      \
	do {                                                                                                               \
		printf("%-35s", #test);                                                                                        \
		test();                                                                                                        \
		puts("OK");                                                                                                    \
	} while (0)

#define ASSERT(expr) assert(expr)

static const size_t DEQUE_N = 20;

static void test_deque_init_destroy(void) {
	deque_of(int) d;
	deque_init(&d);
	// on success, capacity must be 4, head/size zero
	ASSERT(d.data != nullptr);
	ASSERT(d.capacity == 4);
	ASSERT(d.size == 0);
	ASSERT(d.head == 0);

	deque_destroy(&d);

	ASSERT(d.data == nullptr);
	ASSERT(d.capacity == 0);
	ASSERT(d.size == 0);
	ASSERT(d.head == 0);
}

static void test_deque_push_pop_back_and_front(void) {
	deque_of(int) d;
	deque_init(&d);

	// push_back 0..DEQUE_N-1, then pop_front in same order
	for (int i = 0; i < (int)DEQUE_N; i++) {
		ASSERT(deque_push_back(&d, i));
		int *pb = deque_back(&d);
		ASSERT(pb && *pb == i);
	}
	ASSERT(d.size == DEQUE_N);
	for (int i = 0; i < (int)DEQUE_N; i++) {
		int v = deque_pop_front(&d);
		ASSERT(v == i);
	}
	ASSERT(deque_empty(&d));

	// push_front 0..DEQUE_N-1, then pop_back in same order
	for (int i = 0; i < (int)DEQUE_N; i++) {
		ASSERT(deque_push_front(&d, i));
		int *pf = deque_front(&d);
		ASSERT(pf && *pf == i);
	}
	ASSERT(d.size == DEQUE_N);
	for (int i = 0; i < (int)DEQUE_N; i++) {
		int v = deque_pop_back(&d);
		ASSERT(v == i);
	}
	ASSERT(deque_empty(&d));

	deque_destroy(&d);
}

static void test_deque_clear_and_empty_size(void) {
	deque_of(int) d;
	deque_init(&d);

	for (int i = 0; i < (int)DEQUE_N; i++)
		ASSERT(deque_push_back(&d, i));
	ASSERT(!deque_empty(&d));
	ASSERT(deque_size(&d) == DEQUE_N);

	deque_clear(&d);
	ASSERT(deque_empty(&d));
	ASSERT(deque_size(&d) == 0);

	deque_destroy(&d);
}

static void test_deque_reserve_and_get(void) {
	deque_of(int) d;
	deque_init(&d);

	// bump capacity up
	size_t want = 10;
	ASSERT(deque_reserve(&d, want));
	ASSERT(d.capacity >= want);

	// fill and index
	for (int i = 0; i < (int)want; i++)
		ASSERT(deque_push_back(&d, i * 2));
	ASSERT(deque_size(&d) == want);

	for (size_t i = 0; i < want; i++)
		ASSERT(deque_get(&d, i) == (int)(i * 2));

	deque_destroy(&d);
}

static void test_deque_wrap_around(void) {
	deque_of(int) d;
	deque_init(&d);

	// fill buffer so head stays at 0..3
	for (int i = 0; i < 4; i++)
		ASSERT(deque_push_back(&d, i));
	// pop two -> head = 2
	ASSERT(deque_pop_front(&d) == 0);
	ASSERT(deque_pop_front(&d) == 1);
	// push two more -> wrap around into slots 0 and 1
	ASSERT(deque_push_back(&d, 4));
	ASSERT(deque_push_back(&d, 5));
	// now sequence should be 2,3,4,5
	for (int expect = 2; expect < 6; expect++) {
		int v = deque_pop_front(&d);
		ASSERT(v == expect);
	}
	ASSERT(deque_empty(&d));

	deque_destroy(&d);
}

int main(void) {
	puts("\n=== DEQUE Functional Tests ===");
	RUN(test_deque_init_destroy);
	RUN(test_deque_push_pop_back_and_front);
	RUN(test_deque_clear_and_empty_size);
	RUN(test_deque_reserve_and_get);
	RUN(test_deque_wrap_around);

	printf("\nAll deque tests passed successfully!\n");
	return 0;
}
