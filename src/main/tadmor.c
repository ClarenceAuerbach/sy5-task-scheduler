#include <errno.h>
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


string_t *PIPES_DIRECTORY;


/* Send a LIST request and display response */
int handle_list(int req_fd, string_t *rep_pipe_path) {
    int debug_fd = open("/home/clarence/Licence/sy5-task-scheduler/DEBUG", O_WRONLY | O_CREAT | O_APPEND, 0644);
    
    dprintf(debug_fd, "\n=== handle_list START ===\n");
    
    buffer_t *msg = init_buf();
    if (!msg) {
        dprintf(debug_fd, "ERROR: init_buf failed\n");
        close(debug_fd);
        return -1;
    }

    dprintf(debug_fd, "Writing OP_LIST (0x%04X)\n", OP_LIST);
    if (write16(msg, OP_LIST) != 0) {
        dprintf(debug_fd, "ERROR: write16 failed\n");
        free_buf(msg);
        close(debug_fd);
        return -1;
    }

    dprintf(debug_fd, "msg->length=%zu, msg->data=[", msg->length);
    for (size_t i = 0; i < msg->length; i++) {
        dprintf(debug_fd, "%02X ", msg->data[i]);
    }
    dprintf(debug_fd, "]\n");

    dprintf(debug_fd, "Calling write_atomic_chunks to fd %d\n", req_fd);
    if (write_atomic_chunks(req_fd, msg->data, msg->length) != 0) {
        dprintf(debug_fd, "ERROR: write_atomic_chunks failed, errno=%d\n", errno);
        free_buf(msg);
        close(debug_fd);
        return -1;
    }
    dprintf(debug_fd, "Request sent successfully\n");
    free_buf(msg);

    dprintf(debug_fd, "Opening reply pipe: %s\n", rep_pipe_path->data);
    int rep_fd = open(rep_pipe_path->data, O_RDONLY);
    if (rep_fd < 0) {
        dprintf(debug_fd, "ERROR: open reply pipe failed, errno=%d\n", errno);
        perror("open reply pipe");
        close(debug_fd);
        return -1;
    }
    dprintf(debug_fd, "Reply pipe opened, fd=%d\n", rep_fd);
    
    uint16_t anstype;
    dprintf(debug_fd, "Calling read16 for anstype\n");
    int r = read16(rep_fd, &anstype);
    dprintf(debug_fd, "read16 returned %d, anstype=0x%04X\n", r, anstype);
    
    if (r != 0) {
        dprintf(debug_fd, "ERROR: read16 failed\n");
        fprintf(stderr, "Failed to read answer type\n");
        close(rep_fd);
        close(debug_fd);
        return -1;
    }
    
    if (anstype != ANS_OK) {
        dprintf(debug_fd, "ERROR: anstype != ANS_OK (got 0x%04X, expected 0x%04X)\n", anstype, ANS_OK);
        fprintf(stderr, "Error response from daemon\n");
        close(rep_fd);
        close(debug_fd);
        return -1;
    }
    dprintf(debug_fd, "anstype OK\n");
    
    // Read NBTASKS
    uint32_t nbtasks;
    dprintf(debug_fd, "Calling read32 for nbtasks\n");
    if (read32(rep_fd, &nbtasks) != 0) {
        dprintf(debug_fd, "ERROR: read32 nbtasks failed\n");
        fprintf(stderr, "Failed to read nbtasks\n");
        close(rep_fd);
        close(debug_fd);
        return -1;
    }
    dprintf(debug_fd, "nbtasks=%u\n", nbtasks);
    
    // Read and display each task
    for (uint32_t i = 0; i < nbtasks; i++) {
        dprintf(debug_fd, "Reading task %u/%u\n", i+1, nbtasks);
        
        // Read TASKID
        uint64_t taskid;
        dprintf(debug_fd, "  Reading taskid...\n");
        if (read64(rep_fd, &taskid) != 0){
            dprintf(debug_fd, "  ERROR: read64 taskid failed\n");
            close(rep_fd);
            close(debug_fd);
            return -1;
        }
        dprintf(debug_fd, "  taskid=%lu\n", taskid);

        // Read TIMING
        uint64_t minutes;
        uint32_t hours;
        uint8_t days;
        
        dprintf(debug_fd, "  Reading minutes...\n");
        if (read64(rep_fd, &minutes) != 0){
            dprintf(debug_fd, "  ERROR: read64 minutes failed\n");
            close(rep_fd);
            close(debug_fd);
            return -1;
        }
        dprintf(debug_fd, "  minutes=0x%016lX\n", minutes);
        
        dprintf(debug_fd, "  Reading hours...\n");
        if (read32(rep_fd, &hours) != 0){
            dprintf(debug_fd, "  ERROR: read32 hours failed\n");
            close(rep_fd);
            close(debug_fd);
            return -1;
        }
        dprintf(debug_fd, "  hours=0x%08X\n", hours);
        
        dprintf(debug_fd, "  Reading days...\n");
        unsigned char dbuf[1];
        if (read(rep_fd, dbuf, 1) != 1){
            dprintf(debug_fd, "  ERROR: read days failed\n");
            close(rep_fd);
            close(debug_fd);
            return -1;
        }
        days = dbuf[0];
        dprintf(debug_fd, "  days=0x%02X\n", days);
        
        // Read COMMANDLINE
        string_t *cmdline = init_str();
        if (read_command(rep_fd, cmdline) != 0) {
            dprintf(debug_fd, "  ERROR: read_command failed\n");
            free_str(cmdline);
            close(rep_fd);
            close(debug_fd);
            return -1;
        }
        dprintf(debug_fd, "  cmdline='%s'\n", cmdline->data);
        size_t cmdlen = cmdline->length;    

        if (!cmdline) {
            dprintf(debug_fd, "  ERROR: malloc cmdline failed\n");
            close(rep_fd);
            close(debug_fd);
            return -1;
        }
        
        // Lire toute la commandline
        dprintf(debug_fd, "  Reading cmdline (%ld bytes)...\n", cmdlen);
        size_t total = 0;
        while (total < cmdlen) {
            ssize_t n = read(rep_fd, cmdline + total, cmdlen - total);
            dprintf(debug_fd, "    read() returned %zd (total=%zu/%ld)\n", n, total + (n>0?n:0), cmdlen);
            if (n <= 0) {
                dprintf(debug_fd, "  ERROR: read cmdline failed at offset %zu\n", total);
                free(cmdline);
                close(rep_fd);
                close(debug_fd);
                return -1;
            }
            total += n;
        }
        dprintf(debug_fd, "  cmdline='%s'\n", cmdline->data);
        
        // Format and display
        char min_str[256], hrs_str[128], day_str[32];
        
        if (minutes == 0 && hours == 0 && days == 0) {
            snprintf(min_str, sizeof(min_str), "-");
            snprintf(hrs_str, sizeof(hrs_str), "-");
            snprintf(day_str, sizeof(day_str), "-");
        } else {
            bitmap_to_string(minutes, 59, min_str, sizeof(min_str));
            bitmap_to_string(hours, 23, hrs_str, sizeof(hrs_str));
            bitmap_to_string(days, 6, day_str, sizeof(day_str));
        }
        
        dprintf(debug_fd, "  Formatted: %lu: %s %s %s %s\n", taskid, min_str, hrs_str, day_str, cmdline->data);
        printf("%lu: %s %s %s %s\n", taskid, min_str, hrs_str, day_str, cmdline->data);
        fflush(stdout);

        free(cmdline);
    }
    
    dprintf(debug_fd, "=== handle_list SUCCESS ===\n");
    close(rep_fd);
    close(debug_fd);
    return 0;
}

/* Send TIMES_EXITCODES request and display response */
int handle_times_exitcodes(int req_fd, string_t *rep_pipe_path, uint64_t taskid) {
    buffer_t *msg = init_buf();
    if (!msg) return -1;
    
    if (write16(msg, OP_TIMES_EXITCODES) != 0) {
        free_buf(msg);
        return -1;
    }
    
    if (write64(msg, taskid) != 0) {
        free_buf(msg);
        return -1;
    }
    
    if (write_atomic_chunks(req_fd, msg->data, msg->length) != 0) {
        free_buf(msg);
        return -1;
    }
    free_buf(msg);
    
    // Read response

    int rep_fd = open(rep_pipe_path->data, O_RDONLY);

    uint16_t anstype;
    if (read16(rep_fd, &anstype) != 0){
        close(rep_fd);
        return -1;
    }

    if (anstype == ANS_ERROR) {
        uint16_t errcode;
        if (read16(rep_fd, &errcode) != 0) return -1;
        if (errcode == ERR_NOT_FOUND) {
            fprintf(stderr, "Task not found\n");
        } else {
            fprintf(stderr, "Unknown error\n");
        }
        close(rep_fd);
        return -1;
    }
    
    // Read NBRUNS
    uint32_t nbruns;
    if (read32(rep_fd, &nbruns) != 0){
        close(rep_fd);
        return -1;
    }

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
                close(rep_fd);
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
                close(rep_fd);
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
    close(rep_fd);
    return 0;
}
/* Send STDOUT or STDERR request and display response */
int handle_output(int req_fd, string_t *rep_pipe_path, uint64_t taskid, int is_stdout) {
    buffer_t *msg = init_buf();
    
    if (write16(msg, is_stdout ? OP_STDOUT : OP_STDERR) != 0) {
        free_buf(msg);
        return -1;
    }
    
    if (write64(msg, taskid) != 0) {
        free_buf(msg);
        return -1;
    }
    
    if (write_atomic_chunks(req_fd, msg->data, msg->length) != 0) {
        free_buf(msg);
        return -1;
    }
    free_buf(msg);
    

    // Read response

    int rep_fd = open(rep_pipe_path->data, O_RDONLY);

    uint16_t anstype;
    if (read16(rep_fd, &anstype) != 0){
        close(rep_fd);
        return -1;
    }

    if (anstype == ANS_ERROR) {
        uint16_t errcode;
        if (read16(rep_fd, &errcode) != 0) return -1;
        if (errcode == ERR_NOT_FOUND) {
            fprintf(stderr, "Task not found\n");
        } else if (errcode == ERR_NOT_RUN) {
            fprintf(stderr, "Task has not been executed yet\n");
        }

        close(rep_fd);
        return -1;
    }
    
    // Read OUTPUT
    uint32_t len;
    if (read32(rep_fd, &len) != 0) return -1;

    char *output = malloc(len + 1);

    char *p = output;
    size_t remaining = len;

    while (remaining > 0) {
        ssize_t nb = read(rep_fd, p, remaining);
        if (nb == 0) {
            // EOF reached unexpectedly
            fprintf(stderr, "Unexpected EOF\n");
            free(output);
            close(rep_fd);
            return -1;
        }
        remaining -= nb;
        p += nb;
    }

    output[len] = '\0';
    printf("%s", output);
    free(output);    
    close(rep_fd);
    return 0;
}

/* Send REMOVE request */
int handle_remove(int req_fd, string_t *rep_pipe_path, uint64_t taskid) {
    buffer_t *msg = init_buf();
    if (!msg) return -1;
    
    if (write16(msg, OP_REMOVE) != 0) {
        free_buf(msg);
        return -1;
    }
    
    if (write64(msg, taskid) != 0) {
        free_buf(msg);
        return -1;
    }
    
    if (write_atomic_chunks(req_fd, msg->data, msg->length) != 0) {
        free_buf(msg);
        return -1;
    }
    free_buf(msg);
    
    // Read response


    int rep_fd = open(rep_pipe_path->data, O_RDONLY);

    uint16_t anstype;
    if (read16(rep_fd, &anstype) != 0) return -1;
    
    if (anstype == ANS_ERROR) {
        uint16_t errcode;
        if (read16(rep_fd, &errcode) != 0){
            close(rep_fd);
            return -1;
        }

        if (errcode == ERR_NOT_FOUND) {
            fprintf(stderr, "Task not found\n");
        }
        close(rep_fd);
        return -1;
    }
    close(rep_fd);
    return 0;
}

/* Send TERMINATE request */
int handle_terminate(int req_fd, string_t *rep_pipe_path) {
    buffer_t *msg = init_buf();
    if (!msg) return -1;
    
    if (write16(msg, OP_TERMINATE) != 0) {
        free_buf(msg);
        return -1;
    }
    
    if (write_atomic_chunks(req_fd, msg->data, msg->length) != 0) {
        free_buf(msg);
        return -1;
    }
    free_buf(msg);

    // Read response
    int rep_fd = open(rep_pipe_path->data, O_RDONLY);

    uint16_t anstype;
    
    if (read16(rep_fd, &anstype) != 0) {
        close(rep_fd);
        return -1;
    }
    if (anstype != ANS_OK) {
        fprintf(stderr, "Error response from daemon\n");
        close(rep_fd);
        return -1;
    }   
    close(rep_fd);
    return 0;
}

int main(int argc, char **argv) {
    
    int debug_fd = open("/home/clarence/Licence/sy5-task-scheduler/DEBUG", O_WRONLY | O_CREAT | O_APPEND, 0644);
    
    dprintf(debug_fd, "DEBUG: argc=%d\n", argc);
    for (int i = 0; i < argc; i++) {
        dprintf(debug_fd, "DEBUG: argv[%d]=%s\n", i, argv[i]);
    }

    PIPES_DIRECTORY = new_str("/tmp/");
    append(PIPES_DIRECTORY, getenv("USER"));
    append(PIPES_DIRECTORY, "/erraid/pipes");
    /* Check for -p option, before so it doesn't affect getopt  */
    for( int i = 0; i<argc-1 ; i++){
        if (!strcmp(argv[i], "-P")) {
            set_str(PIPES_DIRECTORY, argv[i+1]);
        }
    }

    string_t *req_pipe_path = new_str(PIPES_DIRECTORY->data);
    append(req_pipe_path, "/erraid-request-pipe");
    string_t *rep_pipe_path = new_str(PIPES_DIRECTORY->data);
    append(rep_pipe_path, "/erraid-reply-pipe");

    dprintf(debug_fd, "DEBUG: req_pipe_path=%s\n", req_pipe_path->data);
    dprintf(debug_fd, "DEBUG: rep_pipe_path=%s\n", rep_pipe_path->data);

    int req_fd = open(req_pipe_path->data, O_WRONLY);

    if (req_fd < 0) {
        perror("open req_pipe");
        dprintf(debug_fd, "DEBUG: Failed to open %s\n", req_pipe_path->data);
        return 1;
    }
    
    dprintf(debug_fd, "DEBUG: req_fd opened successfully\n");
    
    int opt;
    int ret = 0;
    opterr = 0; 
    while ((opt = getopt(argc, argv, ":lqr:x:o:e:p:P:")) != -1) {
        switch (opt) {
        case 'l':{
            ret = handle_list(req_fd, rep_pipe_path);
            break;
        }
        case 'q': {
            ret = handle_terminate(req_fd, rep_pipe_path);
            break;
        }

        case 'r': {
            if (!optarg)  goto error;
            uint64_t taskid = strtoull(optarg, NULL, 10);
            ret = handle_remove(req_fd, rep_pipe_path, taskid);
            break;
        }

        case 'x': {
            if (!optarg)  goto error;
            uint64_t taskid = strtoull(optarg, NULL, 10);
            ret = handle_times_exitcodes(req_fd, rep_pipe_path, taskid);
            break;
        }

        case 'o': {
            if (!optarg)  goto error;
            uint64_t taskid = strtoull(optarg, NULL, 10);
            ret = handle_output(req_fd, rep_pipe_path, taskid, 1);
            break;
        }

        case 'e': {
            if (!optarg)  goto error;
            uint64_t taskid = strtoull(optarg, NULL, 10);
            ret = handle_output(req_fd, rep_pipe_path, taskid, 0);
            break;
        }
        case 'P': {
            // Already handled
            break;
        }

        default:
            goto error;
        }
    }
    close(req_fd);  
    free_str(req_pipe_path);
    free_str(rep_pipe_path);
    free_str(PIPES_DIRECTORY);
    return ret;

error :
    close(req_fd);  
    free_str(req_pipe_path);
    free_str(rep_pipe_path);
    free_str(PIPES_DIRECTORY);
    fprintf(stderr, "Error: Unknown option '-%c'\n", optopt);
    fprintf(stderr,
        "Usage: tadmor [-p PATH] [-l|-q|-r TASKID|-x TASKID|-o TASKID|-e TASKID]\n");
    return 1;
}
