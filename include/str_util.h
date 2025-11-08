#ifndef STR_UTIL_H
#define STR_UTIL_H

#include <stddef.h>

// Concatenates the `count` strings in strings
// Result must be freed
char *concat_n(size_t n, char *strings[]);

// Concatenates two strings
// Result must be freed
char *concat(char *s1, char *s2);

#endif
