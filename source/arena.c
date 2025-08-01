#include <arena.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

enum { ALIGNMENT = sizeof(void *) };

static size_t align_up(size_t n) { return (n + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1); }

static struct arena_block *make_block(size_t min_size) {
	struct arena_block *blk = malloc(sizeof *blk);
	if (!blk)
		return nullptr;

	blk->capacity = align_up(min_size);
	blk->used = 0;
	blk->next = nullptr;

	blk->data = malloc(blk->capacity);
	if (!blk->data) {
		free(blk);
		return nullptr;
	}
	memset(blk->data, 0, blk->capacity);
	return blk;
}

bool arena_init(struct arena *arena, size_t default_size) {
	assert(arena != nullptr);

	arena->first = nullptr;
	arena->current = nullptr;
	arena->block_size = (default_size > 0 ? default_size : 1024);

	arena->first = make_block(arena->block_size);
	if (!arena->first)
		return false;

	arena->current = arena->first;
	return true;
}

void *arena_alloc(struct arena *arena, size_t size) {
	assert(arena != nullptr);
	assert(size > 0);

	size_t needed = align_up(size);

	if (arena->current->used + needed <= arena->current->capacity) {
		void *ptr = (char *)arena->current->data + arena->current->used;
		arena->current->used += needed;
		return ptr;
	}

	size_t new_size = arena->block_size;
	if (needed > new_size)
		new_size = needed;

	struct arena_block *blk = make_block(new_size);
	if (!blk)
		return nullptr;

	arena->current->next = blk;
	arena->current = blk;

	blk->used = needed;
	return blk->data;
}

void arena_destroy(struct arena *arena) {
	assert(arena != nullptr);

	struct arena_block *blk = arena->first;
	while (blk) {
		struct arena_block *next = blk->next;
		free(blk->data);
		free(blk);
		blk = next;
	}

	arena->first = nullptr;
	arena->current = nullptr;
	arena->block_size = 0;
}

char *arena_strdup(struct arena *arena, const char *s) {
	assert(arena != nullptr);
	assert(s != nullptr);

	size_t len = strlen(s) + 1;
	char *copy = arena_alloc(arena, len);
	if (!copy)
		return nullptr;

	memcpy(copy, s, len);
	return copy;
}
