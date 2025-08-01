#ifndef STRING_INTERN_H
#define STRING_INTERN_H

/**
 * Interns a string.
 *
 * @param str          string to be interned and copied into interner owned memory.
 * @return             pointer to interned string, nullptr on failure.
 */
const char *intern(const char *str);

/**
 * Frees all memory associated with the string interner and invalidates all given-out string pointers.
 */
void intern_destroy(void);

#endif /* STRING_INTERN_H */
