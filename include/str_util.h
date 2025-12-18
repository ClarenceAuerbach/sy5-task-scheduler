#ifndef STR_UTIL_H
#define STR_UTIL_H

#include <stddef.h>
#include <stdint.h>

// Data: heap allocated data
// Length: number of (non null) characters
// Capacity: bytes allocated for this string
typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} string_t;

// Data: heap allocated data
// Length: number of (non null) characters
// Capacity: bytes allocated for this string
typedef struct {
    uint8_t *data;
    size_t length;
    size_t capacity;
} buffer_t;

// Builds an empty string
string_t *init_str(void);

// Builds an empty buffer
buffer_t *init_buf(void);

// Builds a string from a char*
string_t *new_str(char *s);

void free_str(string_t *string);

void free_buf(buffer_t *buf);

// Returns a pointer to a full copy of string
string_t *copy_str(string_t *string);

// Returns a pointer to a full copy of string
buffer_t *copy_buf(buffer_t *buf);

// Appends a char* to a string
// Returns NULL on error
int append(string_t *dest, const char *s);

int appendn(buffer_t *dest, const void *val, size_t byte_count);

// Removes n characters from the string
void trunc_str_by(string_t *str, size_t n);

// Removes n bytes from the string
void trunc_buf_by(buffer_t *buf, size_t n);

// Removes characters such that the length of the string is n
void trunc_str_to(string_t *str, size_t n);

// Removes bytes such that the length of the buffer is n
void trunc_buf_to(buffer_t *buf, size_t n);

#endif
