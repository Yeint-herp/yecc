#ifndef DEQUE_H
#define DEQUE_H

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/*
 * deque.h
 * Generic double-ended queue (deque) module.
 *
 * Implements a ring-buffer-based deque with amortized O(1) push/pop on both ends.
 */

#define DEQUE_GROW_FACTOR 2

/* declare a deque over elements of type T */
#define deque_of(T)                                                                                                    \
	struct {                                                                                                           \
		T *data;                                                                                                       \
		size_t head;                                                                                                   \
		size_t size;                                                                                                   \
		size_t capacity;                                                                                               \
	}

/* initialize a deque */
#define deque_init(d)                                                                                                  \
	do {                                                                                                               \
		(d)->data = malloc(4 * sizeof *(d)->data);                                                                     \
		(d)->capacity = (d)->data ? 4 : 0;                                                                             \
		(d)->head = 0;                                                                                                 \
		(d)->size = 0;                                                                                                 \
	} while (0)

/* free storage and reset to empty */
#define deque_destroy(d)                                                                                               \
	do {                                                                                                               \
		free((d)->data);                                                                                               \
		(d)->data = nullptr;                                                                                           \
		(d)->capacity = 0;                                                                                             \
		(d)->head = 0;                                                                                                 \
		(d)->size = 0;                                                                                                 \
	} while (0)

/* clear contents but keep capacity */
#define deque_clear(d)                                                                                                 \
	do {                                                                                                               \
		(d)->head = 0;                                                                                                 \
		(d)->size = 0;                                                                                                 \
	} while (0)

#define deque_front(d) ((d)->size ? &(d)->data[(d)->head] : nullptr)
#define deque_back(d) ((d)->size ? &(d)->data[((d)->head + (d)->size - 1) % (d)->capacity] : nullptr)

/* INTERNAL: grow buffer to at least double size (or 4 if empty) */
#define DEQUE_GROW(d)                                                                                                  \
	({                                                                                                                 \
		bool _ok = false;                                                                                              \
		size_t _oldcap = (d)->capacity;                                                                                \
		size_t _newcap = _oldcap ? _oldcap * DEQUE_GROW_FACTOR : 4;                                                    \
		typeof((d)->data) _newbuf = malloc(_newcap * sizeof *(d)->data);                                               \
		if (_newbuf) {                                                                                                 \
			for (size_t _i = 0; _i < (d)->size; ++_i) {                                                                \
				_newbuf[_i] = (d)->data[((d)->head + _i) % _oldcap];                                                   \
			}                                                                                                          \
			free((d)->data);                                                                                           \
			(d)->data = _newbuf;                                                                                       \
			(d)->capacity = _newcap;                                                                                   \
			(d)->head = 0;                                                                                             \
			_ok = true;                                                                                                \
		}                                                                                                              \
		_ok;                                                                                                           \
	})

/* push element at back; returns true on success, false on OOM */
#define deque_push_back(d, elem)                                                                                       \
	({                                                                                                                 \
		bool _ok = true;                                                                                               \
		if ((d)->size == (d)->capacity)                                                                                \
			_ok = DEQUE_GROW(d);                                                                                       \
		if (_ok) {                                                                                                     \
			size_t _idx = ((d)->head + (d)->size) % (d)->capacity;                                                     \
			(d)->data[_idx] = (elem);                                                                                  \
			(d)->size++;                                                                                               \
		}                                                                                                              \
		_ok;                                                                                                           \
	})

/* push element at front; returns true on success, false on OOM */
#define deque_push_front(d, elem)                                                                                      \
	({                                                                                                                 \
		bool _ok = true;                                                                                               \
		if ((d)->size == (d)->capacity)                                                                                \
			_ok = DEQUE_GROW(d);                                                                                       \
		if (_ok) {                                                                                                     \
			(d)->head = (d)->head ? (d)->head - 1 : (d)->capacity - 1;                                                 \
			(d)->data[(d)->head] = (elem);                                                                             \
			(d)->size++;                                                                                               \
		}                                                                                                              \
		_ok;                                                                                                           \
	})

/* pop and return front element (undefined if empty) */
#define deque_pop_front(d)                                                                                             \
	({                                                                                                                 \
		typeof((d)->data[0]) _val = (d)->data[(d)->head];                                                              \
		(d)->head = ((d)->head + 1) % (d)->capacity;                                                                   \
		(d)->size--;                                                                                                   \
		_val;                                                                                                          \
	})

/* pop and return back element (undefined if empty) */
#define deque_pop_back(d)                                                                                              \
	({                                                                                                                 \
		size_t _idx = ((d)->head + (d)->size - 1) % (d)->capacity;                                                     \
		typeof((d)->data[0]) _val = (d)->data[_idx];                                                                   \
		(d)->size--;                                                                                                   \
		_val;                                                                                                          \
	})

/* get element at logical index i (0 <= i < size) */
#define deque_get(d, i) ((d)->data[((d)->head + (i)) % (d)->capacity])

/* ensure capacity >= n; returns true on success, false on OOM */
#define deque_reserve(d, n)                                                                                            \
	({                                                                                                                 \
		bool _ok = true;                                                                                               \
		if ((n) > (d)->capacity) {                                                                                     \
			size_t _oldcap = (d)->capacity;                                                                            \
			typeof((d)->data) _newbuf = malloc((n) * sizeof *(d)->data);                                               \
			if (!_newbuf)                                                                                              \
				_ok = false;                                                                                           \
			else {                                                                                                     \
				for (size_t _i = 0; _i < (d)->size; ++_i)                                                              \
					_newbuf[_i] = (d)->data[((d)->head + _i) % _oldcap];                                               \
				free((d)->data);                                                                                       \
				(d)->data = _newbuf;                                                                                   \
				(d)->capacity = (n);                                                                                   \
				(d)->head = 0;                                                                                         \
			}                                                                                                          \
		}                                                                                                              \
		_ok;                                                                                                           \
	})

#define deque_empty(d) ((d)->size == 0)
#define deque_size(d) ((d)->size)

#endif /* DEQUE_H */
