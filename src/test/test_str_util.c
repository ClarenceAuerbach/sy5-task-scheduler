#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "str_util.h"

int main(void) {
    char *s1 = "Hello";
    char *s2 = " ";
    char *s3 = "World";
    char *s4 = "!";

    string_t *str = new_string(s1);
    assert(!strcmp(str->data, "Hello"));
    assert(str->length == 5);
    assert(str->capacity == 6);

    append(str, s2);
    append(str, s3);
    append(str, s4);
    assert(!strcmp(str->data, "Hello World!"));
    assert(str->length == 12);
    assert(str->capacity >= 13);

    truncate_by(str, 1);
    assert(!strcmp(str->data, "Hello World"));
    assert(str->length == 11);
    assert(str->capacity >= 13);

    truncate_to(str, 5);
    assert(!strcmp(str->data, "Hello"));
    assert(str->length == 5);
    assert(str->capacity >= 13);
}
