#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "str_util.h"

char *concat_n(size_t count, char *strings[]) {
    size_t res_len = 0;
    for (size_t i = 0; i < count; i++) {
        res_len += strlen(strings[i]);
    }
    char *res = malloc(res_len+1);
    char *ptr = res;
    for (size_t i = 0; i < count; i++) {
        strcpy(ptr, strings[i]);
        ptr += strlen(strings[i]);
    }
    return res;
}

char *concat(char *s1, char *s2) {
    size_t res_len = strlen(s1) + strlen(s2) + 1;
    char *res = malloc(res_len);
    char *ptr = res;
    strcpy(ptr, s1);
    ptr += strlen(s1);
    strcpy(ptr, s2);
    return res;
}
