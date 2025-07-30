#ifndef ARENA_H
#define ARENA_H

/*
 * arena.h
 * Basic arena allocator for bulk, cache friendly allocations.
 */

#include <stdbool.h>
#include <stddef.h>

struct arena {
	size_t capacity, used;
	unsigned char *data;
};

/* Initializes arena to be able to allocate initial_size */
bool arena_init(struct arena *arena, size_t initial_size);

/* allocates size bytes from arena and returns a pointer to its start, nullptr on OOM */
void *arena_alloc(struct arena *arena, size_t size);

/* frees all memory owned by arena */
void arena_destroy(struct arena *arena);

/* reallocates the arena to new_capacity, false on failure */
bool arena_realloc(struct arena *arena, size_t new_capacity);

#endif /* ARENA_H */
