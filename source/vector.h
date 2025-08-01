#ifndef VECTOR_H
#define VECTOR_H

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/*
 * vector.h
 * Generic dynamic array (vector) module.
 */

#define VECTOR_GROW_MARGIN 1.8

/* declare a vector over elements of type T */
#define vector_of(T)                                                                                                   \
	struct {                                                                                                           \
		T *data;                                                                                                       \
		size_t size;                                                                                                   \
		size_t capacity;                                                                                               \
	}

/* release heap storage and reset to empty. */
#define vector_destroy(v)                                                                                              \
	do {                                                                                                               \
		free((v)->data);                                                                                               \
		(v)->data = nullptr;                                                                                           \
		(v)->size = 0;                                                                                                 \
		(v)->capacity = 0;                                                                                             \
	} while (0)

/* clear contents but keep allocated capacity. */
#define vector_clear(v)                                                                                                \
	do {                                                                                                               \
		(v)->size = 0;                                                                                                 \
	} while (0)

/* Reserve capacity for at least n elements.
 * Returns true on success, false on OOM.
 */
#define vector_reserve(v, n)                                                                                           \
	({                                                                                                                 \
		bool _ok = true;                                                                                               \
		size_t _req = (n);                                                                                             \
		if (_req > (v)->capacity) {                                                                                    \
			void *_tmp = realloc((v)->data, _req * sizeof *(v)->data);                                                 \
			if (!_tmp)                                                                                                 \
				_ok = false;                                                                                           \
			else {                                                                                                     \
				(v)->data = _tmp;                                                                                      \
				(v)->capacity = _req;                                                                                  \
			}                                                                                                          \
		}                                                                                                              \
		_ok;                                                                                                           \
	})

/* Push one element to the end.
 * Grows if needed. Returns true on success, false on OOM.
 */
#define vector_push(v, elem)                                                                                           \
	({                                                                                                                 \
		bool _ok = true;                                                                                               \
		if ((v)->size == (v)->capacity) {                                                                              \
			size_t _newcap = (v)->capacity ? (v)->capacity * VECTOR_GROW_MARGIN : 4;                                   \
			void *_tmp = realloc((v)->data, _newcap * sizeof *(v)->data);                                              \
			if (!_tmp)                                                                                                 \
				_ok = false;                                                                                           \
			else {                                                                                                     \
				(v)->data = _tmp;                                                                                      \
				(v)->capacity = _newcap;                                                                               \
			}                                                                                                          \
		}                                                                                                              \
		if (_ok)                                                                                                       \
			(v)->data[(v)->size++] = (elem);                                                                           \
		_ok;                                                                                                           \
	})

/* pop and return the last element. If empty, behavior is undefined. */
#define vector_pop(v) ((v)->data[--(v)->size])

/* get (lvalue) element at index i */
#define vector_get(v, i) ((v)->data[(i)])

/* pointer to last element */
#define vector_back(v) ((v)->size ? &(v)->data[(v)->size - 1] : nullptr)

/* Insert element at index i (0 <= i <= size), shifting tail right.
 * Returns true on success, false on OOM.
 */
#define vector_insert(v, i, elem)                                                                                      \
	({                                                                                                                 \
		bool _ok = true;                                                                                               \
		if ((v)->size == (v)->capacity) {                                                                              \
			size_t _newcap = (v)->capacity ? (v)->capacity * 2 : 4;                                                    \
			void *_tmp = realloc((v)->data, _newcap * sizeof *(v)->data);                                              \
			if (!_tmp)                                                                                                 \
				_ok = false;                                                                                           \
			else {                                                                                                     \
				(v)->data = _tmp;                                                                                      \
				(v)->capacity = _newcap;                                                                               \
			}                                                                                                          \
		}                                                                                                              \
		if (_ok) {                                                                                                     \
			memmove(&(v)->data[(i) + 1], &(v)->data[(i)], ((v)->size - (i)) * sizeof *(v)->data);                      \
			(v)->data[(i)] = (elem);                                                                                   \
			(v)->size++;                                                                                               \
		}                                                                                                              \
		_ok;                                                                                                           \
	})

/* erase element at index i (0 <= i < size) shifting tail left. */
#define vector_erase(v, i)                                                                                             \
	do {                                                                                                               \
		memmove(&(v)->data[(i)], &(v)->data[(i) + 1], ((v)->size - (i) - 1) * sizeof *(v)->data);                      \
		(v)->size--;                                                                                                   \
	} while (0)

#define vector_size(v) ((v)->size)
#define vector_capacity(v) ((v)->capacity)
#define vector_empty(v) ((v)->size == 0)

#endif /* VECTOR_H */
