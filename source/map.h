#ifndef MAP_H
#define MAP_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * map.h
 * Generic open-addressing hash map designed for separately allocated key/value storage.
 *
 * Linear probing is used to handle hash conflicts.
 * Keys and values are stored in separate memory blocks for optimial cash layout.
 * Bucket status metadata is bit-packed (2 bits per bucket) to reduce overhead.
 */

#ifdef TRACE_MAP
#define LOG(fmt, ...) printf("[MAP] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define LOG(...) (void)(0 ? (fprintf(stderr, __VA_ARGS__), 0) : 0) // silences compiler warnings on unused variables
#endif

enum bucket_status : uint8_t {
	BUCKET_STATUS_EMPTY = 0b00,	   // The slot has never been used
	BUCKET_STATUS_GRAVE = 0b01,	   // Previously occupied slot which is now removed
	BUCKET_STATUS_FULL = 0b10,	   // Valid key-value entry is present
	BUCKET_STATUS_RESERVED = 0b11, // Reserved for future use
};

/*
 * Retrieves the 2-bit bucket_status value for bucket index i
 * from the compressed status_bits array.
 */
#define GET_STATUS(bits, i)                                                                                            \
	({                                                                                                                 \
		uint8_t _st = (((bits)[(i) >> 2] >> (((i) & 3) * 2)) & 0x3);                                                   \
		LOG("GET_STATUS idx=%zu => %u", (size_t)(i), _st);                                                             \
		_st;                                                                                                           \
	})

/*
 * Sets the 2-bit bucket_status value for bucket index i
 * in the compressed status_bits array to val.
 */
#define SET_STATUS(bits, i, val)                                                                                       \
	do {                                                                                                               \
		uint8_t *b = &(bits)[(i) >> 2];                                                                                \
		uint8_t _old = ((*b >> (((i) & 3) * 2)) & 0x3);                                                                \
		LOG("SET_STATUS idx=%zu from %u to %u", (size_t)(i), _old, (unsigned)((val) & 0x3));                           \
		*b = (*b & ~(0x3 << (((i) & 3) * 2))) | (((val) & 0x3) << (((i) & 3) * 2));                                    \
	} while (0)

/*
 * INTERNAL: linear‑probe find.
 * On exit *out_idx is either
 *   – the slot where key was found (and *out_found == true),
 *   – or the first grave seen (if any), or the first empty bucket (if no grave).
 */
#define MAP_FIND(m, key, out_idx, out_found)                                                                           \
	do {                                                                                                               \
		size_t __map_cap = (m)->capacity;                                                                              \
		uintptr_t __map_hash = (m)->hash(key);                                                                         \
		size_t __map_i = __map_hash % __map_cap;                                                                       \
		size_t __map_first_grave = SIZE_MAX;                                                                           \
		bool __map_found = false;                                                                                      \
		LOG("MAP_FIND start key=%p hash=%zu cap=%zu", (void *)(uintptr_t)(key), (size_t)__map_hash, __map_cap);        \
		for (;;) {                                                                                                     \
			uint8_t __map_status = GET_STATUS((m)->status_bits, __map_i);                                              \
			LOG(" MAP_FIND probe idx=%zu status=%u", __map_i, __map_status);                                           \
			if (__map_status == BUCKET_STATUS_EMPTY) {                                                                 \
				LOG("  -> empty at %zu", __map_i);                                                                     \
				if (__map_first_grave != SIZE_MAX)                                                                     \
					__map_i = __map_first_grave;                                                                       \
				break;                                                                                                 \
			} else if (__map_status == BUCKET_STATUS_FULL) {                                                           \
				LOG("  -> full at %zu, comparing", __map_i);                                                           \
				if ((m)->compare((m)->keys[__map_i], (key))) {                                                         \
					LOG("    -> found match at %zu", __map_i);                                                         \
					__map_found = true;                                                                                \
					break;                                                                                             \
				}                                                                                                      \
			} else { /* grave */                                                                                       \
				LOG("  -> grave at %zu", __map_i);                                                                     \
				if (__map_first_grave == SIZE_MAX)                                                                     \
					__map_first_grave = __map_i;                                                                       \
			}                                                                                                          \
			__map_i = (__map_i + 1) % __map_cap;                                                                       \
		}                                                                                                              \
		*(out_idx) = __map_i;                                                                                          \
		if ((out_found) != nullptr)                                                                                    \
			*(out_found) = __map_found;                                                                                \
		LOG("MAP_FIND done idx=%zu found=%s", __map_i, __map_found ? "yes" : "no");                                    \
	} while (0)

/*
 * INTERNAL: resize or rehash the map to new_capacity.
 * Returns true on success (otherwise leaves 'm' untouched).
 */
#define MAP_RESIZE(m, new_capacity)                                                                                    \
	({                                                                                                                 \
		bool out;                                                                                                      \
		size_t _old_cap = (m)->capacity;                                                                               \
		LOG("RESIZE: old_cap=%zu new_cap=%zu", _old_cap, (size_t)(new_capacity));                                      \
                                                                                                                       \
		typeof(*(m)->keys) *_old_keys = (m)->keys;                                                                     \
		typeof(*(m)->values) *_old_vals = (m)->values;                                                                 \
		uint8_t *_old_stat = (m)->status_bits;                                                                         \
                                                                                                                       \
		size_t _new_bytes = (((new_capacity) * 2) + 7) / 8;                                                            \
		typeof(*(m)->keys) *_keys = malloc((new_capacity) * sizeof *(m)->keys);                                        \
		typeof(*(m)->values) *_vals = malloc((new_capacity) * sizeof *(m)->values);                                    \
		uint8_t *_stat = malloc(_new_bytes);                                                                           \
                                                                                                                       \
		bool _ok = (_keys && _vals && _stat);                                                                          \
		if (_ok) {                                                                                                     \
			memset(_stat, 0, _new_bytes);                                                                              \
			(m)->keys = _keys;                                                                                         \
			(m)->values = _vals;                                                                                       \
			(m)->status_bits = _stat;                                                                                  \
			(m)->capacity = (new_capacity);                                                                            \
			(m)->size = 0;                                                                                             \
			(m)->graves = 0;                                                                                           \
                                                                                                                       \
			LOG("  re-inserting %zu slots", _old_cap);                                                                 \
			for (size_t _i = 0; _i < _old_cap; ++_i) {                                                                 \
				if (GET_STATUS(_old_stat, _i) == BUCKET_STATUS_FULL) {                                                 \
					typeof(*_old_keys) _k = _old_keys[_i];                                                             \
					typeof(*_old_vals) _v = _old_vals[_i];                                                             \
					size_t _j;                                                                                         \
					bool _f;                                                                                           \
					MAP_FIND((m), _k, &_j, &_f);                                                                       \
					(m)->keys[_j] = _k;                                                                                \
					(m)->values[_j] = _v;                                                                              \
					SET_STATUS((m)->status_bits, _j, BUCKET_STATUS_FULL);                                              \
					(m)->size++;                                                                                       \
					LOG("    moved old[%zu] -> new[%zu]", _i, _j);                                                     \
				}                                                                                                      \
			}                                                                                                          \
			free(_old_keys);                                                                                           \
			free(_old_vals);                                                                                           \
			free(_old_stat);                                                                                           \
			LOG("RESIZE succeeded, new size=%zu", (m)->size);                                                          \
			out = true;                                                                                                \
		} else {                                                                                                       \
			free(_keys);                                                                                               \
			free(_vals);                                                                                               \
			free(_stat);                                                                                               \
			LOG("RESIZE failed: OOM");                                                                                 \
			out = false;                                                                                               \
		}                                                                                                              \
		out;                                                                                                           \
	})

/*
 * INTERNAL: if load‐ or grave‐ thresholds exceeded, resize/rehash.
 */
#define MAP_MAYBE_REHASH(m)                                                                                            \
	({                                                                                                                 \
		bool _ok = true;                                                                                               \
		LOG("MAYBE_REHASH size=%zu cap=%zu graves=%zu", (m)->size, (m)->capacity, (m)->graves);                        \
		if ((m)->size > (size_t)((m)->capacity * MAP_MAX_LOAD_FACTOR)) {                                               \
			LOG(" load-factor > %.2f, doubling", MAP_MAX_LOAD_FACTOR);                                                 \
			_ok = MAP_RESIZE((m), (m)->capacity * 2);                                                                  \
		} else if ((m)->graves > (size_t)((m)->capacity * MAP_MAX_GRAVE_RATIO)) {                                      \
			LOG(" grave-ratio > %.2f, rehashing same cap", MAP_MAX_GRAVE_RATIO);                                       \
			_ok = MAP_RESIZE((m), (m)->capacity);                                                                      \
		}                                                                                                              \
		_ok;                                                                                                           \
	})

#define MAP_DEFAULT_CAPACITY 16	 // Starting number of buckets
#define MAP_MAX_LOAD_FACTOR 0.75 // Resize when #size exceeds this ratio of #capacity
#define MAP_MAX_GRAVE_RATIO 0.2	 // Rehash when #graves grow too numerous

/* declares a hashmap with key type K and value type V */
#define map_of(K, V)                                                                                                   \
	struct {                                                                                                           \
		K *keys;                                                                                                       \
		V *values;                                                                                                     \
		uint8_t *status_bits;                                                                                          \
		size_t capacity, size, graves;                                                                                 \
		bool (*compare)(K a, K b);                                                                                     \
		uintptr_t (*hash)(K a);                                                                                        \
	}

/* initialized a hashmap to the default capacity with a given key compare and key hash function */
#define map_init(m, cmp, hfn)                                                                                          \
	({                                                                                                                 \
		__label__ free_vals;                                                                                           \
		__label__ free_keys;                                                                                           \
		__label__ done;                                                                                                \
		LOG("map_init: start");                                                                                        \
		bool _ok = false;                                                                                              \
		const size_t _cap = MAP_DEFAULT_CAPACITY;                                                                      \
		(m)->keys = malloc(_cap * sizeof *(m)->keys);                                                                  \
		if (!(m)->keys) {                                                                                              \
			LOG("map_init: keys malloc failed");                                                                       \
			goto done;                                                                                                 \
		}                                                                                                              \
		(m)->values = malloc(_cap * sizeof *(m)->values);                                                              \
		if (!(m)->values) {                                                                                            \
			LOG("map_init: values malloc failed");                                                                     \
			goto free_keys;                                                                                            \
		}                                                                                                              \
		size_t _bytes = ((_cap * 2) + 7) / 8;                                                                          \
		(m)->status_bits = malloc(_bytes);                                                                             \
		if (!(m)->status_bits) {                                                                                       \
			LOG("map_init: status_bits malloc failed");                                                                \
			goto free_vals;                                                                                            \
		}                                                                                                              \
		memset((m)->status_bits, 0, _bytes);                                                                           \
		(m)->capacity = _cap;                                                                                          \
		(m)->size = 0;                                                                                                 \
		(m)->graves = 0;                                                                                               \
		(m)->compare = (cmp);                                                                                          \
		(m)->hash = (hfn);                                                                                             \
		_ok = true;                                                                                                    \
		LOG("map_init: success cap=%zu", _cap);                                                                        \
		goto done;                                                                                                     \
free_vals:                                                                                                             \
		free((m)->values);                                                                                             \
free_keys:                                                                                                             \
		free((m)->keys);                                                                                               \
done:                                                                                                                  \
		_ok;                                                                                                           \
	})

/*
 * map_put:      insert or overwrite;
 *               returns 0 if new insertion,
 *                       1 if you just overwrote an existing key,
 *                       2 if OOM during possible resize.
 */
#define map_put(m, key, value)                                                                                         \
	({                                                                                                                 \
		LOG("map_put: key=%p value=%p", (void *)(uintptr_t)(key), (void *)(uintptr_t)(value));                         \
		int _out;                                                                                                      \
		size_t _idx;                                                                                                   \
		bool _fnd = false;                                                                                             \
		MAP_FIND((m), (key), &_idx, &_fnd);                                                                            \
		if (_fnd) {                                                                                                    \
			LOG(" map_put: overwrite at %zu", _idx);                                                                   \
			(m)->values[_idx] = (value);                                                                               \
			_out = 1;                                                                                                  \
		} else {                                                                                                       \
			if (!MAP_MAYBE_REHASH((m))) {                                                                              \
				LOG(" map_put: resize failed");                                                                        \
				_out = 2;                                                                                              \
			} else {                                                                                                   \
				MAP_FIND((m), (key), &_idx, &_fnd);                                                                    \
				if (GET_STATUS((m)->status_bits, _idx) == BUCKET_STATUS_GRAVE) {                                       \
					(m)->graves--;                                                                                     \
					LOG(" map_put: reusing grave at %zu", _idx);                                                       \
				}                                                                                                      \
				(m)->keys[_idx] = (key);                                                                               \
				(m)->values[_idx] = (value);                                                                           \
				SET_STATUS((m)->status_bits, _idx, BUCKET_STATUS_FULL);                                                \
				(m)->size++;                                                                                           \
				LOG(" map_put: inserted at %zu, new size=%zu", _idx, (m)->size);                                       \
				_out = 0;                                                                                              \
			}                                                                                                          \
		}                                                                                                              \
		_out;                                                                                                          \
	})

/*
 * map_get:  returns pointer to the value, or nullptr if not found
 */
#define map_get(m, key)                                                                                                \
	({                                                                                                                 \
		LOG("map_get: key=%p", (void *)(uintptr_t)(key));                                                              \
		size_t _idx;                                                                                                   \
		bool _fnd = false;                                                                                             \
		MAP_FIND((m), (key), &_idx, &_fnd);                                                                            \
		typeof((m)->values) _res;                                                                                      \
		if (_fnd) {                                                                                                    \
			LOG(" map_get: found at %zu", _idx);                                                                       \
			_res = &((m)->values[_idx]);                                                                               \
		} else {                                                                                                       \
			LOG(" map_get: not found");                                                                                \
			_res = nullptr;                                                                                            \
		}                                                                                                              \
		_res;                                                                                                          \
	})

/*
 * map_remove:  if key present, mark grave and return true; else false.
 */
#define map_remove(m, key)                                                                                             \
	({                                                                                                                 \
		LOG("map_remove: key=%p", (void *)(uintptr_t)(key));                                                           \
		size_t _idx;                                                                                                   \
		bool _fnd = false;                                                                                             \
		MAP_FIND((m), (key), &_idx, &_fnd);                                                                            \
		bool _res;                                                                                                     \
		if (!_fnd) {                                                                                                   \
			LOG(" map_remove: not found");                                                                             \
			_res = false;                                                                                              \
		} else {                                                                                                       \
			SET_STATUS((m)->status_bits, _idx, BUCKET_STATUS_GRAVE);                                                   \
			(m)->size--;                                                                                               \
			(m)->graves++;                                                                                             \
			LOG(" map_remove: removed at %zu, new size=%zu", _idx, (m)->size);                                         \
			_res = true;                                                                                               \
		}                                                                                                              \
		_res;                                                                                                          \
	})

#define map_foreach(m, kvar, vvar)                                                                                     \
	for (size_t __i = 0; __i < (m)->capacity; ++__i)                                                                   \
		if (GET_STATUS((m)->status_bits, __i) == BUCKET_STATUS_FULL &&                                                 \
			(((kvar) = (m)->keys[__i]), ((vvar) = (m)->values[__i]), true))

/*
 * map_clear: reset to empty (keeps the current capacity)
 */
#define map_clear(m)                                                                                                   \
	do {                                                                                                               \
		LOG("map_clear: clearing all buckets");                                                                        \
		memset((m)->status_bits, 0, (((m)->capacity * 2) + 7) / 8);                                                    \
		(m)->size = 0;                                                                                                 \
		(m)->graves = 0;                                                                                               \
	} while (0)

/*
 * map_destroy: free all storage and zero out the struct
 */
#define map_destroy(m)                                                                                                 \
	do {                                                                                                               \
		LOG("map_destroy: freeing storage");                                                                           \
		free((m)->keys);                                                                                               \
		free((m)->values);                                                                                             \
		free((m)->status_bits);                                                                                        \
		(m)->keys = nullptr;                                                                                           \
		(m)->values = nullptr;                                                                                         \
		(m)->status_bits = nullptr;                                                                                    \
		(m)->capacity = (m)->size = (m)->graves = 0;                                                                   \
		(m)->compare = nullptr;                                                                                        \
		(m)->hash = nullptr;                                                                                           \
	} while (0)

#define map_size(m) ((m)->size)
#define map_capacity(m) ((m)->capacity)

#endif /* MAP_H */
