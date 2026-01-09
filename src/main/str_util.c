#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "str_util.h"

// Builds an empty string
string_t *init_str(void) {
    string_t *res = malloc(sizeof(string_t));
    res->data = NULL;
    res->capacity = 0;
    res->length = 0;
    return res;
}

// Builds an empty buffer
buffer_t *init_buf(void) {
    buffer_t *res = malloc(sizeof(buffer_t));
    res->data = NULL;
    res->capacity = 0;
    res->length = 0;
    return res;
}

string_t *new_str(char *s) {
    string_t *res = malloc(sizeof(string_t));
    if (!res) return NULL;

    size_t len = strlen(s);
    res->length = len;
    res->capacity = len+1;

    res->data = malloc(len+1);
    if (!res->data) {
        free(res);
        return NULL;
    }
    memcpy(res->data, s, len);
    res->data[len]='\0';

    return res;
}

void free_str(string_t *string) {
    if (string) {
        if (string->data) free(string->data);
        free(string);
    }
}

void free_buf(buffer_t *buf) {
    if (buf) {
        if (buf->data) free(buf->data);
        free(buf);
    }
}

string_t *copy_str(string_t *string) {
    string_t *res = malloc(sizeof(string_t));
    if (!res) return NULL;
    
    res->data = malloc(string->capacity);
    if (!res->data) {
        free(res);
        return NULL;
    }
    
    res->capacity = string->capacity;
    res->length = string->length;
    memcpy(res->data, string->data, string->length + 1);
    return res;
}

buffer_t *copy_buf(buffer_t *buf) {
    buffer_t *res = malloc(sizeof(buffer_t));
    if (!res) return NULL;
    res->data = malloc(buf->capacity);
    if (!res->data) {
        free(res);
        return NULL;
    }
    res->capacity = buf->capacity;
    res->length = buf->length;
    memcpy(res->data, buf->data, buf->length);
    return res;
}

int append(string_t *dest, const char *s) {
    size_t src_len = strlen(s);
    size_t min_capacity = dest->length + src_len + 1;
    if (dest->capacity < min_capacity) {
        size_t new_capacity = 2 * dest->capacity;
        if (new_capacity < min_capacity) new_capacity = min_capacity;
        char *new_data = realloc(dest->data, new_capacity);
        if (!new_data) return -1;
        dest->data = new_data;
        dest->capacity = new_capacity;
    }

    memcpy(dest->data + dest->length, s, src_len+1);
    dest->length += src_len;

    return 0;
}

int appendn(buffer_t *dest, const void *val, size_t n) {
    size_t min_capacity = dest->length + n;

    if (dest->capacity < min_capacity) {
        size_t new_capacity = 2 * dest->capacity;
        if (new_capacity < min_capacity) new_capacity = min_capacity;
        uint8_t *new_data = realloc(dest->data, new_capacity);
        if (!new_data) return -1;
        dest->data = new_data;
        dest->capacity = new_capacity;
    }

    memcpy(dest->data + dest->length, val, n);
    dest->length += n;
    return 0;
}

// Removes n characters from the string
void trunc_str_by(string_t *str, size_t n) {
    if (n > str->length) n = str->length;
    char *end_of_data = str->data + (str->length - n);
    memset(end_of_data, '\0', n);
    str->length -= n;
}

// Removes n bytes from the string
void trunc_buf_by(buffer_t *buf, size_t n) {
    if (n > buf->length) n = buf->length;
    uint8_t *end_of_data = buf->data + (buf->length - n);
    memset(end_of_data, '\0', n);
    buf->length -= n;
}

// Removes characters such that the length of the string is n
void trunc_str_to(string_t *str, size_t n) {
    memset(str->data+n, 0, str->capacity-n);
    str->length = n;
}

// Removes bytes such that the length of the buffer is n
void trunc_buf_to(buffer_t *buf, size_t n) {
    uint8_t *end_of_data = buf->data + n;
    memset(end_of_data, '\0', n);
    buf->length -= n;
}

// Replaces the string in str with s
void set_str(string_t *str, char *s) {
    trunc_str_to(str, 0);
    append(str, s);
}