#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "str_util.h"
#include "colours.h"

int main(void) {
    char s1[] = "Hello ";
    char *s2 = malloc(sizeof("World") + 1);
    s2 = "World";
    char *s3 = "!";

    char *strings[] = {s1, s2, s3};
    char *res1 = concat_n(3, strings);
    assert(!strcmp(res1, "Hello World!"));
    char *res2 = concat(s1, s2);
    assert(!strcmp(res2, "Hello World"));
    return 0;
}
