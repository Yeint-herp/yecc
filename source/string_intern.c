#include <arena.h>
#include <assert.h>
#include <map.h>
#include <string.h>
#include <string_intern.h>

/* struct holding a key in the string interner hashmap */
struct string_intern_key {
	const char *string;
	size_t len;
};

static struct arena string_intern_arena = {};
map_of(struct string_intern_key, const char *) string_intern_map = {};

/* implemented according to https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function */
/* thanks to struct string_intern_key we save on strlen per hash */
static uintptr_t si_hash_string(const struct string_intern_key sid) {
	constexpr uintptr_t fnv1a_offset_basis = 0xcbf29ce484222325;
	constexpr uintptr_t fnv1a_prime = 0x100000001b3;

	uintptr_t hash = fnv1a_offset_basis;
	for (size_t i = 0; i < sid.len; i++) {
		hash ^= sid.string[i];
		hash *= fnv1a_prime;
	}

	return hash;
}

static bool si_compare_string(const struct string_intern_key a, const struct string_intern_key b) {
	if (a.len != b.len)
		return false;
	if (memcmp(a.string, b.string, a.len) == 0)
		return true;
	return false;
}

const char *intern(const char *str) {
	assert(str != nullptr);

	if (__builtin_expect(string_intern_arena.first == nullptr, 0)) {
		arena_init(&string_intern_arena, 4096);
		map_init(&string_intern_map, si_compare_string, si_hash_string);
	}

	const size_t len = strlen(str);
	const struct string_intern_key sid = {.len = len, .string = str};

	const char **found = map_get(&string_intern_map, sid);
	if (!found) {
		char *underlying = arena_strdup(&string_intern_arena, str);

		int res = map_put(&string_intern_map, sid, underlying);
		assert(res != MAP_PUT_OVERWRITE && "Somehow overwrote a value not found in lookup (race condition?)");
		assert(res != MAP_PUT_OOM && "OOM when trying to resize hashmap!");

		return underlying;
	} else {
		return *found;
	}
}

void intern_destroy(void) {
	map_destroy(&string_intern_map);
	arena_destroy(&string_intern_arena);
}
