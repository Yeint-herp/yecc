#ifndef ARENA_H
#define ARENA_H

#include <stdbool.h>
#include <stddef.h>

/*
 * arena.h
 *
 * Segmented arena allocator for fast, linear allocation
 * without ever invalidating earlier pointers (important!).
 *
 */

struct arena_block {
	void *data;
	size_t capacity;
	size_t used;
	struct arena_block *next;
};

struct arena {
	struct arena_block *first;
	struct arena_block *current;
	size_t block_size;
};

/**
 * Initialize an arena.
 *
 * @param arena        Non-null pointer to arena struct to initialize.
 * @param default_size Minimum size for each new block.
 * @return             true on success, false if initial malloc fails.
 */
bool arena_init(struct arena *arena, size_t default_size);

/**
 * Allocate `size` bytes from the arena.
 *
 * @param arena Non-null pointer to an initialized arena.
 * @param size  Number of bytes to allocate; must be >0.
 * @return      Pointer to the allocated memory, or nullptr on OOM.
 */
void *arena_alloc(struct arena *arena, size_t size);

/**
 * Destroy an arena and free all its blocks.
 *
 * @param arena Non-null pointer to arena previously passed to arena_init.
 *              After call, arena is reset as-if zeroed out.
 */
void arena_destroy(struct arena *arena);

/**
 * Duplicate a null-terminated string into the arena.
 *
 * @param arena Non-null pointer to an initialized arena.
 * @param s     Null-terminated string to duplicate.
 * @return      Pointer to duplicated string in arena, or nullptr on OOM.
 */
char *arena_strdup(struct arena *arena, const char *s);

#endif // ARENA_H
