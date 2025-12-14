#ifndef STR_UTIL_H
#define STR_UTIL_H

#include <stddef.h>

// Data: heap allocated data
// Length: number of (non null) characters
// Capacity: bytes allocated for this string
typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} string_t;

// Builds a string (heap allocated data!) from a char
string_t *new_string(char *s);

void free_string(string_t *string);

// Returns a pointer to a full copy of string
string_t *copy_string(string_t *string);

// Appends a char* to a string
// Returns NULL on error
int append(string_t *dest, const char *s);

// Removes n characters from the string
void truncate_by(string_t *str, int n);

// Removes characters such that the length of the string is n
void truncate_to(string_t *str, int n);

#endif
