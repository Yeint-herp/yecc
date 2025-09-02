#include <assert.h>
#include <base/arena.h>
#include <base/map.h>
#include <base/string_intern.h>
#include <string.h>

/* struct holding a key in the string interner hashmap */
struct string_intern_key {
	const char *string;
	size_t len;
};

static struct arena string_intern_arena = {};
map_of(struct string_intern_key, const char *) string_intern_map = {};

/* implemented according to https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function */
/* thanks to struct string_intern_key we save one strlen per hash */
static uintptr_t si_hash_string(const struct string_intern_key sid) {
	constexpr uintptr_t fnv1a_offset_basis = 0xcbf29ce484222325;
	constexpr uintptr_t fnv1a_prime = 0x100000001b3;

	uintptr_t hash = fnv1a_offset_basis;
	const uint8_t *p = (const uint8_t *)sid.string;
	for (size_t i = 0; i < sid.len; i++) {
		hash ^= (uintptr_t)p[i];
		hash *= fnv1a_prime;
	}

	return hash;
}

static bool si_compare_string(const struct string_intern_key a, const struct string_intern_key b) {
	return a.len == b.len && memcmp(a.string, b.string, a.len) == 0;
}

void intern_init() {
	if (string_intern_arena.first == nullptr)
		arena_init(&string_intern_arena, 4096);
	if (string_intern_map.keys == nullptr)
		map_init(&string_intern_map, si_compare_string, si_hash_string);
}

const char *intern_n(const char *str, size_t len) {
	assert(str != nullptr);

	const struct string_intern_key probe = {.len = len, .string = str};
	const char **found = map_get(&string_intern_map, probe);
	if (found)
		return *found;

	char *underlying = arena_strdup(&string_intern_arena, str);
	struct string_intern_key key = {.string = underlying, .len = len};

	int res = map_put(&string_intern_map, key, underlying);
	assert(res != MAP_PUT_OVERWRITE && "Somehow overwrote a value not found in lookup (race condition?)");
	assert(res != MAP_PUT_OOM && "OOM when trying to resize hashmap!");

	return underlying;
}

const char *intern(const char *str) { return intern_n(str, strlen(str)); }

void intern_destroy(void) {
	map_destroy(&string_intern_map);
	arena_destroy(&string_intern_arena);
}
