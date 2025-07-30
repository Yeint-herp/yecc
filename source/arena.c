#include <arena.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static constexpr size_t alignment = sizeof(void *);  // align to pointer size

bool arena_init(struct arena *arena, size_t initial_size) {
	assert(arena != nullptr);

	size_t aligned = (initial_size + alignment - 1) & ~(alignment - 1);

	arena->data = malloc(aligned);
	memset(arena->data, 0, aligned);
	if (!arena->data) {
		arena->capacity = 0;
		arena->used = 0;
		return false;
	}
	arena->capacity = aligned;
	arena->used = 0;
	return true;
}

void *arena_alloc(struct arena *arena, size_t size) {
	assert(arena != nullptr);
	assert(size != 0);

	size_t aligned = (size + alignment - 1) & ~(alignment - 1);

	if (arena->used + aligned > arena->capacity)
		return nullptr; // OOM

	void *ptr = arena->data + arena->used;
	arena->used += aligned;
	return ptr;
}

void arena_destroy(struct arena *arena) {
	if (arena->data)
		free(arena->data);

	arena->data = nullptr;
	arena->capacity = 0;
	arena->used = 0;
}

bool arena_realloc(struct arena *arena, size_t new_capacity) {
	assert(arena != nullptr);

	if (new_capacity <= arena->capacity)
		return true; // already large enough

	unsigned char *new_data = realloc(arena->data, new_capacity);
	if (!new_data)
		return false;

	arena->data = new_data;
	arena->capacity = new_capacity;
	return true;
}
