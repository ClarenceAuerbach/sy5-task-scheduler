#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <endian.h>
#include <stdio.h>
#include <ctype.h>

#include "tube_util.h"
#include "str_util.h"

int write_atomic_chunks(int fd, char *s, size_t len) {
    while (len > 0) {
        size_t chunk = len > PIPE_BUF ? PIPE_BUF : len;
        int w = write(fd, s, chunk);
        if (w < 0) return -1;
        s += w;
        len -= w;
    }
    return 0;
}

int write16(string_t *msg, uint16_t val) {
    uint16_t be_val = htobe16(val);
    append16(msg, be_val);
    return 0;
}

int write32(string_t *msg, uint32_t val) {
    uint32_t be_val = htobe32(val);
    append32(msg, be_val);
    return 0;
}

int write64(string_t *msg, uint64_t val) {
    uint64_t be_val = htobe64(val);
    append64(msg, be_val);
    return 0;
}

int read16(int fd, uint16_t *val) {
    char buf[2];
    ssize_t r = read(fd, buf, 2);
    
    if (r != 2) {
        printf("%zd\n", r);
        perror("read ok");
        return -1;
    }
    
    uint16_t be_val;
    memcpy(&be_val, buf, 2);
    *val = be16toh(be_val);
    return 0;
}

int read32(int fd, uint32_t *val) {
    unsigned char buf[4];
    ssize_t r = read(fd, buf, 4);
    if (r != 4) return -1;
    
    uint32_t be_val;
    memcpy(&be_val, buf, 4);
    *val = be32toh(be_val);
    return 0;
}

int read64(int fd, uint64_t *val) {
    unsigned char buf[8];
    ssize_t r = read(fd, buf, 8);
    if (r != 8) return -1;
    
    uint64_t be_val;
    memcpy(&be_val, buf, 8);
    *val = be64toh(be_val);
    return 0;
}

uint64_t parse_timing_field(const char *str, int max_value) {
    if (!str) return 0;
    
    if (strcmp(str, "*") == 0) {
        uint64_t result = 0;
        for (int i = 0; i <= max_value; i++) {
            result |= (1ULL << i);
        }
        return result;
    }
    
    uint64_t bitmap = 0;
    char *str_copy = strdup(str);
    if (!str_copy) return 0;
    
    char *token = strtok(str_copy, ",");
    while (token) {

        while (isspace(*token)) token++;
        
        int num = atoi(token);
        if (num >= 0 && num <= max_value) {
            bitmap |= (1ULL << num);
        }
        token = strtok(NULL, ",");
    }
    
    free(str_copy);
    return bitmap;
}

int write_timing(string_t *msg, const char *minutes, const char *hours, 
                 const char *days, int no_timing) {
    if (no_timing) {
        if (write64(msg, 0) != 0) return -1;
        if (write32(msg, 0) != 0) return -1;
        char zero = 0;
        if (append(msg, &zero) != 0) return -1;
        return 0;
    }
    
    // Parser et encoder les minutes (0-59)
    uint64_t minutes_bitmap;
    if (!minutes) {
        // Par défaut: toutes les minutes
        minutes_bitmap = 0;
        for (int i = 0; i < 60; i++) {
            minutes_bitmap |= (1ULL << i);
        }
    } else {
        minutes_bitmap = parse_timing_field(minutes, 59);
    }
    if (write64(msg, minutes_bitmap) != 0) return -1;
    
    // Parser et encoder les heures (0-23)
    uint32_t hours_bitmap;
    if (!hours) {
        // Par défaut: toutes les heures
        hours_bitmap = 0;
        for (int i = 0; i < 24; i++) {
            hours_bitmap |= (1U << i);
        }
    } else {
        hours_bitmap = (uint32_t)parse_timing_field(hours, 23);
    }
    if (write32(msg, hours_bitmap) != 0) return -1;
    
    // Parser et encoder les jours (0-6: dimanche-samedi)
    uint8_t days_bitmap;
    if (!days) {
        // Par défaut: tous les jours
        days_bitmap = 0x7F;  // bits 0-6 à 1
    } else {
        days_bitmap = (uint8_t)parse_timing_field(days, 6);
    }
    char day_byte = days_bitmap;
    if (append(msg, &day_byte) != 0) return -1;
    
    return 0;
}

int write_arguments(string_t *msg, int argc, char **argv) {
     
    if (write32(msg, argc) != 0) return -1;
    
     
    for (int i = 0; i < argc; i++) {
        uint32_t len = strlen(argv[i]);
         
        if (write32(msg, len) != 0) return -1;
        
        for (size_t j = 0; j < len; j++) {
            if (append(msg, &argv[i][j]) != 0) return -1;
        }
    }
    
    return 0;
}

int open_pipes(const char *pipes_dir, int *req_fd, int *rep_fd) {
    string_t *path = new_string("");
    if (!path) return -1;
    
    if (!pipes_dir) {
        char *user = getenv("USER");
        if (!user) {
            fprintf(stderr, "USER environment variable not set\n");
            free_string(path);
            return -1;
        }
        append(path, "/tmp/");
        append(path, user);
        append(path, "/erraid/pipes");
    } else {
        append(path, pipes_dir);
        append(path, "/pipes");
    }
    
    size_t base_len = path->length;
    
    // Ouvrir le pipe de requête
    append(path, "/erraid-request-pipe");

    *req_fd = open(path->data, O_WRONLY );

    if (*req_fd < 0) {
        perror("open request pipe");
        free_string(path);
        return -1;
    }
    
    
    // Ouvrir le pipe de réponse
    truncate_to(path, base_len);
    append(path, "/erraid-reply-pipe");

    *rep_fd = open(path->data, O_RDONLY | O_NONBLOCK);

    if (*rep_fd < 0) {
        perror("open reply pipe");
        close(*req_fd);
        free_string(path);
        return -1;
    }
    
    free_string(path);
    return 0;
}

// À ajouter dans tube_util.c
void bitmap_to_string(uint64_t bitmap, int max_val, char *buf, size_t bufsize) {
    int all_set = 1;
    for (int i = 0; i <= max_val; i++) {
        if (!((bitmap >> i) & 1)) {
            all_set = 0;
            break;
        }
    }
    
    if (all_set) {
        snprintf(buf, bufsize, "*");
        return;
    }
    
    buf[0] = '\0';
    int first = 1;
    for (int i = 0; i <= max_val; i++) {
        if ((bitmap >> i) & 1) {
            if (!first) strncat(buf, ",", bufsize - strlen(buf) - 1);
            char num[16];
            snprintf(num, sizeof(num), "%d", i);
            strncat(buf, num, bufsize - strlen(buf) - 1);
            first = 0;
        }
    }
}
