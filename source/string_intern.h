#ifndef STRING_INTERN_H
#define STRING_INTERN_H

#include <stddef.h>

/**
 * Interns a string.
 *
 * @param str          string to be interned and copied into interner owned memory.
 * @return             pointer to interned string, nullptr on failure.
 */
const char *intern(const char *str);

/**
 * Interns a sized string.
 *
 * @param str          string to be interned and copied into interner owned memory.
 * @param len          length of the given string.
 * @return             pointer to interned string, nullptr on failure.
 */
const char *intern_n(const char *str, size_t len);

/**
 * Frees all memory associated with the string interner and invalidates all given-out string pointers.
 */
void intern_destroy(void);

/**
 * Initializes or reinitializes the string interner.
 */
void intern_init(void);

#endif /* STRING_INTERN_H */
