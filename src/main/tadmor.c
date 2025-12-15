#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <endian.h>
#include <time.h>

#include "str_util.h"
#include "tube_util.h"
#include <bits/getopt_core.h>

/* Send a LIST request and display response */
int handle_list(int req_fd, int rep_fd) {
    string_t *msg = new_string("");
    if (!msg) return -1;
    
    if (write16(msg, OP_LIST) != 0) {
        free_string(msg);
        return -1;
    }
    
    if (write_atomic_chunks(req_fd, msg->data, msg->length) != 0) {
        free_string(msg);
        return -1;
    }
    free_string(msg);
    
    // Read response
    uint16_t anstype;
    if (read16(rep_fd, &anstype) != 0) {
        fprintf(stderr, "Failed to read answer type\n");
        return -1;
    }
    
    if (anstype != ANS_OK) {
        fprintf(stderr, "Error response from daemon\n");
        return -1;
    }
    
    // Read NBTASKS
    uint32_t nbtasks;
    if (read32(rep_fd, &nbtasks) != 0) {
        fprintf(stderr, "Failed to read nbtasks\n");
        return -1;
    }
    
    // Read and display each task
    for (uint32_t i = 0; i < nbtasks; i++) {
        // Read TASKID
        uint64_t taskid;
        if (read64(rep_fd, &taskid) != 0) return -1;
        
        // Read TIMING (8 bytes minutes + 4 bytes hours + 1 byte days)
        uint64_t minutes;
        uint32_t hours;
        uint8_t days;
        
        if (read64(rep_fd, &minutes) != 0) return -1;
        if (read32(rep_fd, &hours) != 0) return -1;
        
        // Lire le byte des jours
        unsigned char dbuf[1];
        if (read(rep_fd, dbuf, 1) != 1) return -1;
        days = dbuf[0];
        
        // Read COMMANDLINE
        uint32_t cmdlen;
        if (read32(rep_fd, &cmdlen) != 0) return -1;
        
        char *cmdline = malloc(cmdlen + 1);
        if (!cmdline) return -1;
        
        // Lire toute la commandline
        size_t total = 0;
        while (total < cmdlen) {
            ssize_t n = read(rep_fd, cmdline + total, cmdlen - total);
            if (n <= 0) {
                free(cmdline);
                return -1;
            }
            total += n;
        }
        cmdline[cmdlen] = '\0';
        
        // Format and display
        char min_str[256], hrs_str[128], day_str[32];
        
        // Check if timing is null (no execution)
        if (minutes == 0 && hours == 0 && days == 0) {
            snprintf(min_str, sizeof(min_str), "-");
            snprintf(hrs_str, sizeof(hrs_str), "-");
            snprintf(day_str, sizeof(day_str), "-");
        } else {
            // Convert bitmaps to strings
            bitmap_to_string(minutes, 59, min_str, sizeof(min_str));
            bitmap_to_string(hours, 23, hrs_str, sizeof(hrs_str));
            bitmap_to_string(days, 6, day_str, sizeof(day_str));
        }
        
        printf("%lu: %s %s %s %s\n", taskid, min_str, hrs_str, day_str, cmdline);
        free(cmdline);
    }
    
    return 0;
}

/* Send TIMES_EXITCODES request and display response */
int handle_times_exitcodes(int req_fd, int rep_fd, uint64_t taskid) {
    string_t *msg = new_string("");
    if (!msg) return -1;
    
    if (write16(msg, OP_TIMES_EXITCODES) != 0) {
        free_string(msg);
        return -1;
    }
    
    if (write64(msg, taskid) != 0) {
        free_string(msg);
        return -1;
    }
    
    if (write_atomic_chunks(req_fd, msg->data, msg->length) != 0) {
        free_string(msg);
        return -1;
    }
    free_string(msg);
    
    // Read response
    uint16_t anstype;
    if (read16(rep_fd, &anstype) != 0) return -1;
    
    if (anstype == ANS_ERROR) {
        uint16_t errcode;
        if (read16(rep_fd, &errcode) != 0) return -1;
        if (errcode == ERR_NOT_FOUND) {
            fprintf(stderr, "Task not found\n");
        } else {
            fprintf(stderr, "Unknown error\n");
        }
        return -1;
    }
    
    // Read NBRUNS
    uint32_t nbruns;
    if (read32(rep_fd, &nbruns) != 0) return -1;
    
    // Read and display each run
    for (uint32_t i = 0; i < nbruns; i++) {
        // Lire 8 bytes pour timestamp
        uint64_t be_timestamp;
        unsigned char time_buf[8];
        size_t total = 0;
        while (total < 8) {
            ssize_t n = read(rep_fd, time_buf + total, 8 - total);
            if (n <= 0) {
                fprintf(stderr, "Error reading timestamp\n");
                return -1;
            }
            total += n;
        }
        memcpy(&be_timestamp, time_buf, 8);
        uint64_t timestamp = be64toh(be_timestamp);
        
        uint16_t be_exitcode;
        unsigned char exit_buf[2];
        total = 0;
        while (total < 2) {
            ssize_t n = read(rep_fd, exit_buf + total, 2 - total);
            if (n <= 0) {
                fprintf(stderr, "Error reading exitcode\n");
                return -1;
            }
            total += n;
        }
        memcpy(&be_exitcode, exit_buf, 2);
        uint16_t exitcode = be16toh(be_exitcode);
        
        // Convert timestamp to local time
        time_t t = (time_t)timestamp;
        struct tm *tm = localtime(&t);
        if (!tm) {
            fprintf(stderr, "Error converting time for timestamp %lu\n", timestamp);
            continue;
        }
        
        // Format: YYYY-MM-DD HH:MM:SS exitcode
        printf("%04d-%02d-%02d %02d:%02d:%02d %u\n",
               tm->tm_year + 1900, 
               tm->tm_mon + 1, 
               tm->tm_mday,
               tm->tm_hour, 
               tm->tm_min, 
               tm->tm_sec,
               exitcode);
    }
    
    return 0;
}
/* Send STDOUT or STDERR request and display response */
int handle_output(int req_fd, int rep_fd, uint64_t taskid, int is_stdout) {
    string_t *msg = new_string("");
    
    if (write16(msg, is_stdout ? OP_STDOUT : OP_STDERR) != 0) {
        free_string(msg);
        return -1;
    }
    
    if (write64(msg, taskid) != 0) {
        free_string(msg);
        return -1;
    }
    
    if (write_atomic_chunks(req_fd, msg->data, msg->length) != 0) {
        free_string(msg);
        return -1;
    }
    free_string(msg);
    

    // Read response
    uint16_t anstype;
    if (read16(rep_fd, &anstype) != 0) return -1;
    
    if (anstype == ANS_ERROR) {
        uint16_t errcode;
        if (read16(rep_fd, &errcode) != 0) return -1;
        if (errcode == ERR_NOT_FOUND) {
            fprintf(stderr, "Task not found\n");
        } else if (errcode == ERR_NOT_RUN) {
            fprintf(stderr, "Task has not been executed yet\n");
        }
        return -1;
    }
    
    // Read OUTPUT
    uint32_t len;
    if (read32(rep_fd, &len) != 0) return -1;

    char *output = malloc(len + 1);
    if (!output) return -1;

    char *p = output;
    size_t remaining = len;

    while (remaining > 0) {
        ssize_t nb = read(rep_fd, p, remaining);
        if (nb == 0) {
            // EOF reached unexpectedly
            fprintf(stderr, "Unexpected EOF\n");
            free(output);
            return -1;
        }
        remaining -= nb;
        p += nb;
    }

    output[len] = '\0';
    printf("%s", output);
    free(output);
    
    return 0;
}

/* Send REMOVE request */
int handle_remove(int req_fd, int rep_fd, uint64_t taskid) {
    string_t *msg = new_string("");
    if (!msg) return -1;
    
    if (write16(msg, OP_REMOVE) != 0) {
        free_string(msg);
        return -1;
    }
    
    if (write64(msg, taskid) != 0) {
        free_string(msg);
        return -1;
    }
    
    if (write_atomic_chunks(req_fd, msg->data, msg->length) != 0) {
        free_string(msg);
        return -1;
    }
    free_string(msg);
    
    // Read response
    uint16_t anstype;
    if (read16(rep_fd, &anstype) != 0) return -1;
    
    if (anstype == ANS_ERROR) {
        uint16_t errcode;
        if (read16(rep_fd, &errcode) != 0) return -1;
        if (errcode == ERR_NOT_FOUND) {
            fprintf(stderr, "Task not found\n");
        }
        return -1;
    }
    
    return 0;
}

/* Send TERMINATE request */
int handle_terminate(int req_fd, int rep_fd) {
    string_t *msg = new_string("");
    if (!msg) return -1;
    
    if (write16(msg, OP_TERMINATE) != 0) {
        free_string(msg);
        return -1;
    }
    
    if (write_atomic_chunks(req_fd, msg->data, msg->length) != 0) {
        free_string(msg);
        return -1;
    }
    free_string(msg);

    // Read response
    uint16_t anstype;
    if (read16(rep_fd, &anstype) != 0) return -1;
    
    return 0;
}

int handle_pipe_dir(int *req_fd, int *rep_fd, char * path){
    if (open_pipes(path, req_fd, rep_fd) != 0) {
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: tadmor [-c|-l|-r|-x|-o|-e|-s|-q] ...\n");
        return 1;
    }

    int opt;
    int ret = 0;
    int req_fd = -1;
    int rep_fd = -1;

    /* Check for -p option */
    for( int i = 0; i<argc ; i++){
        if (!strcmp(argv[i], "-p")) {
            ret = handle_pipe_dir(&req_fd, &rep_fd, argv[i+1]);
            if (ret != 0) {
                return ret;
            }
            break;    
        }
    }

    if (req_fd == -1 || rep_fd == -1) {
        int ret = open_pipes(NULL, &req_fd, &rep_fd);
        if(ret) return -1;
    }

    /* Blocks response pipe*/
    int flags = fcntl(rep_fd, F_GETFL);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        goto mainend;
    }

    if (fcntl(rep_fd, F_SETFL, flags & ~O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
        goto mainend;
    }
    

    while ((opt = getopt(argc, argv, "lqr:x:o:e:p:")) != -1) {
        
        switch (opt) {
        case 'l':
            ret = handle_list(req_fd, rep_fd);
            break;

        case 'q':
            ret = handle_terminate(req_fd, rep_fd);
            break;

        case 'r': {
            uint64_t taskid = strtoull(optarg, NULL, 10);
            ret = handle_remove(req_fd, rep_fd, taskid);
            break;
        }

        case 'x': {
            uint64_t taskid = strtoull(optarg, NULL, 10);
            ret = handle_times_exitcodes(req_fd, rep_fd, taskid);
            break;
        }

        case 'o': {
            uint64_t taskid = strtoull(optarg, NULL, 10);
            ret = handle_output(req_fd, rep_fd, taskid, 1);
            break;
        }

        case 'e': {
            uint64_t taskid = strtoull(optarg, NULL, 10);
            ret = handle_output(req_fd, rep_fd, taskid, 0);
            break;
        }
        case 'p': {
            break;
        }

        default:
            fprintf(stderr,
                "Usage: tadmor [-p PATH] [-l|-q|-r TASKID|-x TASKID|-o TASKID|-e TASKID]\n");
            return 1;
        }
    }
    
mainend:

    close(req_fd);
    close(rep_fd);

    return ret;
}