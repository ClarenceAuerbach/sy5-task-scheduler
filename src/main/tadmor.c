#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>

#include "str_util.h"
#include "tadmor_util.h"

/**
 * Send a LIST request
 * Returns 0 on success, -1 on error
 */
int send_list_request(int req_fd) {
    string_t *msg = new_string("");
    if (!msg) return -1;
    
    if (write_uint16(msg, OP_LIST) != 0) {
        free_string(msg);
        return -1;
    }
    
    int r = write_atomic_chunks(req_fd, msg->data, msg->length);
    free_string(msg);
    return r;
}

/**
 * Send a REMOVE request
 * Returns 0 on success, -1 on error
 */
int send_remove_request(int req_fd, uint64_t taskid) {
    string_t *msg = new_string("");
    if (!msg) return -1;
    
    if (write_uint16(msg, OP_REMOVE) != 0) {
        free_string(msg);
        return -1;
    }
    
    if (write_uint64(msg, taskid) != 0) {
        free_string(msg);
        return -1;
    }
    
    int r = write_atomic_chunks(req_fd, msg->data, msg->length);
    free_string(msg);
    return r;
}

/**
 * Send a TIMES_EXITCODES request
 * Returns 0 on success, -1 on error
 */
int send_times_exitcodes_request(int req_fd, uint64_t taskid) {
    string_t *msg = new_string("");
    if (!msg) return -1;
    
    if (write_uint16(msg, OP_TIMES_EXITCODES) != 0) {
        free_string(msg);
        return -1;
    }
    
    if (write_uint64(msg, taskid) != 0) {
        free_string(msg);
        return -1;
    }
    
    int r = write_atomic_chunks(req_fd, msg->data, msg->length);
    free_string(msg);
    return r;
}

/**
 * Send a STDOUT request
 * Returns 0 on success, -1 on error
 */
int send_stdout_request(int req_fd, uint64_t taskid) {
    string_t *msg = new_string("");
    if (!msg) return -1;
    
    if (write_uint16(msg, OP_STDOUT) != 0) {
        free_string(msg);
        return -1;
    }
    
    if (write_uint64(msg, taskid) != 0) {
        free_string(msg);
        return -1;
    }
    
    int r = write_atomic_chunks(req_fd, msg->data, msg->length);
    free_string(msg);
    return r;
}

/**
 * Send a STDERR request
 * Returns 0 on success, -1 on error
 */
int send_stderr_request(int req_fd, uint64_t taskid) {
    string_t *msg = new_string("");
    if (!msg) return -1;
    
    if (write_uint16(msg, OP_STDERR) != 0) {
        free_string(msg);
        return -1;
    }
    
    if (write_uint64(msg, taskid) != 0) {
        free_string(msg);
        return -1;
    }
    
    int r = write_atomic_chunks(req_fd, msg->data, msg->length);
    free_string(msg);
    return r;
}

/**
 * Send a TERMINATE request
 * Returns 0 on success, -1 on error
 */
int send_terminate_request(int req_fd) {
    string_t *msg = new_string("");
    if (!msg) return -1;
    
    if (write_uint16(msg, OP_TERMINATE) != 0) {
        free_string(msg);
        return -1;
    }
    
    int r = write_atomic_chunks(req_fd, msg->data, msg->length);
    free_string(msg);
    return r;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: tadmor [-c|-l|-r|-x|-o|-e|-s|-q] ...\n");
        return 1;
    }
    
    int req_fd, rep_fd;
    
    if (open_pipes(NULL, &req_fd, &rep_fd) != 0) {
        return 1;
    }

    // Handle -l (LIST)
    if (strcmp(argv[1], "-l") == 0) {
        int r = send_list_request(req_fd);
        close(req_fd);
        close(rep_fd);
        return r;
    }

    // Handle -q (TERMINATE)
    if (strcmp(argv[1], "-q") == 0) {
        int r = send_terminate_request(req_fd);
        close(req_fd);
        close(rep_fd);
        return r;
    }

    // All other options require a TASKID
    if (argc < 3) {
        fprintf(stderr, "%s requires TASKID\n", argv[1]);
        close(req_fd);
        close(rep_fd);
        return 1;
    }

    // Convert TASKID from string to uint64
    uint64_t taskid = strtoull(argv[2], NULL, 10);
    int r = -1;

    // Handle -r (REMOVE)
    if (strcmp(argv[1], "-r") == 0) {
        r = send_remove_request(req_fd, taskid);
    }
    // Handle -x (TIMES_EXITCODES)
    else if (strcmp(argv[1], "-x") == 0) {
        r = send_times_exitcodes_request(req_fd, taskid);
    }
    // Handle -o (STDOUT)
    else if (strcmp(argv[1], "-o") == 0) {
        r = send_stdout_request(req_fd, taskid);
    }
    // Handle -e (STDERR)
    else if (strcmp(argv[1], "-e") == 0) {
        r = send_stderr_request(req_fd, taskid);
    }
    // Invalid option
    else {
        fprintf(stderr, "Invalid option: %s\n", argv[1]);
        close(req_fd);
        close(rep_fd);
        return 1;
    }

    close(req_fd);
    close(rep_fd);
    return r;
}