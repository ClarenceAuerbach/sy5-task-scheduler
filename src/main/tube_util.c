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
#include "task.h"

int write_atomic_chunks(int fd, uint8_t *s, size_t len) {
    while (len > 0) {
        size_t chunk = len > PIPE_BUF ? PIPE_BUF : len;
        int w = write(fd, s, chunk);
        if (w < 0) return -1;
        s += w;
        len -= w;
    }
    return 0;
}

int write16(buffer_t *msg, uint16_t val) {
    uint16_t be_val = htobe16(val);
    appendn(msg, &be_val, 2);
    return 0;
}

int write32(buffer_t *msg, uint32_t val) {
    uint32_t be_val = htobe32(val);
    appendn(msg, &be_val, 4);
    return 0;
}

int write64(buffer_t *msg, uint64_t val) {
    uint64_t be_val = htobe64(val);
    appendn(msg, &be_val, 8);
    return 0;
}

int read16(int fd, uint16_t *val) {
    unsigned char buf[2];
    size_t total = 0;

    while (total < 2) {
        ssize_t r = read(fd, buf + total, 2 - total);
        if (r < 0) {
            perror("read16");
            return -1;
        }
        if (r == 0) {
            // EOF
            fprintf(stderr, "read16: unexpected EOF\n");
            return -1;
        }
        total += r;
    }
    
    uint16_t be_val;
    memcpy(&be_val, buf, 2);
    *val = be16toh(be_val);
    return 0;
}

int read32(int fd, uint32_t *val) {
    unsigned char buf[4];
    size_t total = 0;
    
    while (total < 4) {
        ssize_t r = read(fd, buf + total, 4 - total);
        if (r < 0) {
            perror("read32");
            return -1;
        }
        if (r == 0) {
            // EOF
            fprintf(stderr, "read32: unexpected EOF\n");
            return -1;
        }
        total += r;
    }
    
    uint32_t be_val;
    memcpy(&be_val, buf, 4);
    *val = be32toh(be_val);
    return 0;
}

int read64(int fd, uint64_t *val) {
    unsigned char buf[8];
    size_t total = 0;
    
    while (total < 8) {
        ssize_t r = read(fd, buf + total, 8 - total);
        if (r < 0) {
            perror("read64");
            return -1;
        }
        if (r == 0) {
            // EOF
            fprintf(stderr, "read64: unexpected EOF\n");
            return -1;
        }
        total += r;
    }
    
    uint64_t be_val;
    memcpy(&be_val, buf, 8);
    *val = be64toh(be_val);
    return 0;
}

int read_command(int fd, string_t *result) {
    uint16_t type;
    if (read16(fd, &type) != 0) return -1;
    
    /* Reading SI type commands */
    if (type == U16('S','I')) {  
        uint32_t argc;
        if (read32(fd, &argc) != 0) return -1;
        
        for (uint32_t i = 0; i < argc; i++) {
            if (i > 0) append(result, " ");
            
            uint32_t len;
            if (read32(fd, &len) != 0) return -1;
            
            char *arg = malloc(len + 1);
            if (!arg) return -1;
            
            size_t total = 0;
            while (total < len) {
                ssize_t n = read(fd, arg + total, len - total);
                if (n <= 0) {
                    free(arg);
                    return -1;
                }
                total += n;
            }
            arg[len] = '\0';
            
            append(result, arg);
            free(arg);
        }
    
    /* Formating/Reading SQ and PL type commands */
    } else if (type == U16('S','Q') || type == U16('P','L')) {  
        uint32_t nbcmds;
        if (read32(fd, &nbcmds) != 0) return -1;
        
        append(result, "(");
        
        for (uint32_t i = 0; i < nbcmds; i++) {
            if (i > 0) append(result, (type == U16('P','L'))? " | ":" ; ");
            
            string_t *subcmd = init_str();
            if (read_command(fd, subcmd) != 0) {
                free_str(subcmd);
                return -1;
            }
            append(result, subcmd->data);
            free_str(subcmd);
        }
        
        append(result, ")");
        
    } else if (type == U16('I','F') ){ 
        /*
         *(if cmd1; then cmd2; else cmd3; fi)
         * Otherwise, executes (if cmd1; then cmd2; fi)
         */


        uint32_t argc;
        if (read32(fd, &argc) != 0) return -1;
        if( argc == 3 || argc == 2){
            
            append(result, "(if ");
            
            string_t *subcmd = init_str();
            if (read_command(fd, subcmd) != 0) {
                free_str(subcmd);
                return -1;
            }
            append(result, subcmd->data);
            free_str(subcmd);

            append(result, "; then ");
            subcmd = init_str();
            if (read_command(fd, subcmd) != 0) {
                free_str(subcmd);
                return -1;
            }
            append(result, subcmd->data);
            free_str(subcmd);

            /* When there's only two commands. */
            if( argc == 2){
                append(result, "; fi)");
                return 0;
            }

            append(result, "; else ");
            subcmd = init_str();
            if (read_command(fd, subcmd) != 0) {
                free_str(subcmd);
                return -1;
            }
            append(result, subcmd->data);
            free_str(subcmd);

            append(result, "; fi)");
        }else{
            fprintf(stderr, "Wrong number of commands for IF type command.");
        }

    }
    return 0;
}

int write_command(buffer_t *msg, command_t *cmd) {
    if (!strcmp(cmd->type, "SI")) {
        
        write16(msg, 0x5349);
        
        write32(msg, cmd->args.argc);
        
        for (uint32_t i = 0; i < cmd->args.argc; i++) {
            write32(msg, (uint32_t)cmd->args.argv[i].length);
            appendn(msg, cmd->args.argv[i].data, cmd->args.argv[i].length);
        }
        
    } else if (!strcmp(cmd->type, "SQ") || !strcmp(cmd->type, "PL") || !strcmp(cmd->type, "IF")) {
        
        write16(msg, U16(cmd->type[0],cmd->type[1]));

        write32(msg, cmd->nbcmds);
        
        for (uint32_t i = 0; i < cmd->nbcmds; i++) {
            write_command(msg, &cmd->cmd[i]);
        }
        
    } else {
        fprintf(stderr, "Unknown command type: %s\n", cmd->type);
        return -1;
    }
    
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

int write_timing(buffer_t *msg, const char *minutes, const char *hours, 
                 const char *days, int no_timing) {
    if (no_timing) {
        if (write64(msg, 0) != 0) return -1;
        if (write32(msg, 0) != 0) return -1;
        return 0;
    }
    
    uint64_t minutes_bitmap;
    if (!minutes) {
        minutes_bitmap = 0;
        for (int i = 0; i < 60; i++) {
            minutes_bitmap |= (1ULL << i);
        }
    } else {
        minutes_bitmap = parse_timing_field(minutes, 59);
    }
    if (write64(msg, minutes_bitmap) != 0) return -1;
    
    uint32_t hours_bitmap;
    if (!hours) {
        hours_bitmap = 0;
        for (int i = 0; i < 24; i++) {
            hours_bitmap |= (1U << i);
        }
    } else {
        hours_bitmap = (uint32_t)parse_timing_field(hours, 23);
    }
    if (write32(msg, hours_bitmap) != 0) return -1;
    
    uint8_t days_bitmap;
    if (!days) {
        days_bitmap = 0x7F;  // bits 0-6 à 1
    } else {
        days_bitmap = (uint8_t)parse_timing_field(days, 6);
    }
    char day_byte = days_bitmap;
    if (appendn(msg, &day_byte, 1) != 0) return -1;
    
    return 0;
}

int write_arguments(buffer_t *msg, int argc, char **argv) {
    if (write32(msg, argc) != 0) return -1;
    
     
    for (int i = 0; i < argc; i++) {
        uint32_t len = strlen(argv[i]);

        if (write32(msg, len) != 0) return -1;
        appendn(msg, argv[i], len);
    }
    
    return 0;
}

void bitmap_to_string(uint64_t bitmap, int max_val, char *buf, size_t bufsize) {
    // Check if all bits are set
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

    for (int i = 0; i <= max_val; ) {
        if (!((bitmap >> i) & 1)) {
            i++;
            continue;
        }

        int start = i;
        int end = i;

        while (end + 1 <= max_val && ((bitmap >> (end + 1)) & 1)) {
            end++;
        }

        if (!first)
            strncat(buf, ",", bufsize - strlen(buf) - 1);

        char tmp[32];
        if (start == end) {
            snprintf(tmp, sizeof(tmp), "%d", start);
        } else {
            snprintf(tmp, sizeof(tmp), "%d-%d", start, end);
        }

        strncat(buf, tmp, bufsize - strlen(buf) - 1);
        first = 0;

        i = end + 1;
    }
}

static uint64_t parse_bitmap(const char *s, int max_val) {
    uint64_t bitmap = 0;

    if (strcmp(s, "*") == 0) {
        for (int i = 0; i <= max_val; i++)
            bitmap |= (1ULL << i);
        return bitmap;
    }

    char *copy = strdup(s);
    char *tok = strtok(copy, ",");

    while (tok) {
        int start, end;

        if (sscanf(tok, "%d-%d", &start, &end) == 2) {
            if (start > end) {
                int tmp = start;
                start = end;
                end = tmp;
            }
            if (start < 0) start = 0;
            if (end > max_val) end = max_val;

            for (int i = start; i <= end; i++)
                bitmap |= (1ULL << i);

        } else if (sscanf(tok, "%d", &start) == 1) {
            if (start >= 0 && start <= max_val)
                bitmap |= (1ULL << start);
        }

        tok = strtok(NULL, ",");
    }

    free(copy);
    return bitmap;
}

uint64_t str_min_to_bitmap(const char *s) {
    return parse_bitmap(s, 59);
}

uint32_t str_hours_to_bitmap(const char *s) {
    return (uint32_t)parse_bitmap(s, 23);
}

uint8_t str_days_to_bitmap(const char *s) {
    return (uint8_t)parse_bitmap(s, 6);
}