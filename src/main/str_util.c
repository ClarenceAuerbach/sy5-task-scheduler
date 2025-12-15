#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "str_util.h"

string_t *new_string(char *s) {
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

void free_string(string_t *string) {
    if (string) {
        if (string->data) free(string->data);
        free(string);
    }
}

string_t *copy_string(string_t *string) {
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

int appendn(string_t *dest, const char *s, int n) {
    size_t min_capacity = dest->length + n;
    
    if (dest->capacity < min_capacity) {
        size_t new_capacity = 2 * dest->capacity;
        if (new_capacity < min_capacity) new_capacity = min_capacity;
        char *new_data = realloc(dest->data, new_capacity);
        if (!new_data) return -1;
        dest->data = new_data;
        dest->capacity = new_capacity;
    }

    memcpy(dest->data + dest->length, &s, n);
    dest->length += n;
    return 0;
}

int append16(string_t *dest, uint16_t s) {
    size_t min_capacity = dest->length + 2 ;
    
    if (dest->capacity < min_capacity) {
        size_t new_capacity = 2 * dest->capacity;
        if (new_capacity < min_capacity) new_capacity = min_capacity;
        char *new_data = realloc(dest->data, new_capacity);
        if (!new_data) return -1;
        dest->data = new_data;
        dest->capacity = new_capacity;
    }

    memcpy(dest->data + dest->length, &s, 2);
    dest->length += 2;
    return 0;
}

int append32(string_t *dest, uint32_t s) {
    size_t min_capacity = dest->length + 4 ;
    if (dest->capacity < min_capacity) {
        size_t new_capacity = 2 * dest->capacity;
        if (new_capacity < min_capacity) new_capacity = min_capacity;
        char *new_data = realloc(dest->data, new_capacity);
        if (!new_data) return -1;
        dest->data = new_data;
        dest->capacity = new_capacity;
    }

    memcpy(dest->data + dest->length, &s, 4);
    dest->length += 4;

    return 0;
}
int append64(string_t *dest, uint64_t s) {
    size_t min_capacity = dest->length + 8 ;
    if (dest->capacity < min_capacity) {
        size_t new_capacity = 2 * dest->capacity;
        if (new_capacity < min_capacity) new_capacity = min_capacity;
        char *new_data = realloc(dest->data, new_capacity);
        if (!new_data) return -1;
        dest->data = new_data;
        dest->capacity = new_capacity;
    }

    memcpy(dest->data + dest->length, &s, 8);
    dest->length += 8;

    return 0;
}

// Removes n characters from the string
void truncate_by(string_t *str, int n) {
    str->data[str->length - n] = '\0';
    str->length -= n;
}

// Removes characters such that the length of the string is n
void truncate_to(string_t *str, int n) {
    str->data[n] = '\0';
    str->length = n;
}
