#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>

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

int main(int argc, char **argv) {
    // Opening pipes
    string_t *path = new_string("");
    char *user = getenv("USER");
    append(path, "/tmp/");
    append(path, user);
    append(path, "/erraid/pipes/erraid-request-pipe");
    int req_fd = open(path->data, O_WRONLY);
    if (req_fd < 0) {
        perror("open pipe");
        return -1;
    }
    truncate_by(path, 19);
    append(path, "erraid-reply-pipe");
    int rep_fd = open(path->data, O_RDONLY);
    if (rep_fd < 0) {
        perror("open pipe");
        return -1;
    }
    free_string(path);

    if (argc < 2) {
        fprintf(stderr, "Usage:\n"
                        "  tadmor -l\n"
                        "  tadmor -x ID\n"
                        "  tadmor -o ID\n"
                        "  tadmor -e ID\n");
        return 1;
    }

    string_t *msg = new_string("");
    if (strcmp(argv[1], "-l") == 0) {
        append(msg, "LS");
        int r = write_atomic_chunks(req_fd, msg->data, 2);
        free_string(msg);
        return r;
    }

    if (argc < 3) {
        fprintf(stderr, "%s requires TASKID\n", argv[1]);
        free_string(msg);
        return 1;
    }

    if (strcmp(argv[1], "-x") == 0) append(msg, "TX");
    else if (strcmp(argv[1], "-o") == 0) append(msg, "STDOUT");
    else if (strcmp(argv[1], "-e") == 0) append(msg, "STDERR");

    if (msg->length == 0) {
        fprintf(stderr, "Invalid option.\n");
        free_string(msg);
        return 1;
    }

    char *id = argv[2];
    // TODO: convert id to uint64
    // int r = write_atomic_chunks();
    free_string(msg);
    // return r;
}
