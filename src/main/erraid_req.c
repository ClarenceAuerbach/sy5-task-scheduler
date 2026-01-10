#include <assert.h>
#include <bits/types/timer_t.h>
#include <dirent.h>
#include <endian.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "str_util.h"
#include "tube_util.h"
#include "task.h"
#include "timing_t.h"

int find_task_index(task_array_t *tasks, uint64_t taskid) {
    for (int i = 0; i < tasks->length; i++) {
        if (tasks->tasks[i]->id == taskid) {
            return i;
        }
    }
    return -1;
}

void command_to_string(command_t *cmd, string_t *result) {
    if (!strcmp(cmd->type, "SI")) {
        for (uint32_t i = 0; i < cmd->args.argc; i++) {
            if (i > 0) append(result, " ");
            append(result, cmd->args.argv[i].data);
        }
    } else if (!strcmp(cmd->type, "SQ")) {
        append(result, "(");
        for (uint32_t i = 0; i < cmd->nbcmds; i++) {
            if (i > 0) append(result, "; ");
            command_to_string(&cmd->cmd[i], result);
        }
        append(result, ")");
    }
}

int handle_request(int req_fd, string_t* rep_pipe_path, task_array_t *tasks, string_t *tasks_path, int status_fd) {
    uint16_t opcode;
    buffer_t *reply = init_buf();
    int r = read16(req_fd, &opcode);
    if (r == -1) {
        perror("Reading opcode");
        return -1;
    }
    
    printf("Received opcode: 0x%04x\n", opcode);
    
    switch(opcode) {
        case OP_LIST: {
            write16(reply, ANS_OK);
            
            write32(reply, (uint32_t)tasks->length);
            
            for (int i = 0; i < tasks->length; i++) {
                write64(reply, tasks->tasks[i]->id);
                
                // TIMING
                timing_t *t = &tasks->tasks[i]->timings;
                write64(reply, t->minutes);   // uint64 big-endian
                write32(reply, t->hours);      // uint32 big-endian
                
                char days_byte = (char)t->daysofweek;
                appendn(reply, &days_byte, 1);
                
                string_t *cmdline = new_str("");
                command_to_string(tasks->tasks[i]->command, cmdline);

                write32(reply, (uint32_t)cmdline->length);
                
                appendn(reply, cmdline->data, cmdline->length);
                
                free_str(cmdline);
            }
            break;
        }

        case OP_TIMES_EXITCODES: {
            uint64_t taskid;
            if (read64(req_fd, &taskid) < 0) {
                return -1;
            }
            
            int idx = find_task_index(tasks, taskid);
            if (idx < 0) {
                // Task not found
                write16(reply, ANS_ERROR);
                write16(reply, ERR_NOT_FOUND);
                break;
            }
            
            char te_path[PATH_MAX];
            snprintf(te_path, PATH_MAX, "%s/%ld/times-exitcodes", 
                    tasks_path->data, taskid);
            
            int te_fd = open(te_path, O_RDONLY);
            if (te_fd < 0) {
                write16(reply, ANS_OK);
                write32(reply, (uint32_t)0);
                break;
            }
            
            struct stat st;
            if (fstat(te_fd, &st) < 0) {
                close(te_fd);
                write16(reply, ANS_ERROR);
                write16(reply, ERR_NOT_FOUND);
                break;
            }
            
            uint32_t nbruns = st.st_size / 10;
            
            write16(reply, ANS_OK);
            write32(reply, nbruns);
            
            unsigned char buf[4096];
            off_t remaining = st.st_size;
            
            while (remaining > 0) {
                size_t to_read = remaining > (int)sizeof(buf) ? (int)sizeof(buf) : remaining;
                ssize_t nb = read(te_fd, buf, to_read);
                if (nb <= 0) {
                    if (nb < 0) perror("read times-exitcodes");
                    close(te_fd);
                    return -1;
                }
                appendn(reply, (char*)buf, nb);
                remaining -= nb;
            }
            
            close(te_fd);
            break;
        }

        case OP_STDOUT:
        case OP_STDERR: {
            uint64_t taskid;
            if (read64(req_fd, &taskid) < 0) {
                free_buf(reply);
                return -1;
            }
            int idx = find_task_index(tasks, taskid);
            if (idx < 0) {
                write16(reply, ANS_ERROR);
                write16(reply, ERR_NOT_FOUND);
                break;
            }

            char path[PATH_MAX];
            snprintf(path, PATH_MAX, "%s/%ld/%s", 
                     tasks_path->data, taskid, 
                     opcode == OP_STDOUT ? "stdout" : "stderr");

            int fd = open(path, O_RDONLY);
            if (fd < 0) {
                write16(reply, ANS_ERROR);
                write16(reply, ERR_NOT_RUN);
                break;
            }

            buffer_t *output = init_buf();
            char buf[1024];
            int n;
            while ((n = read(fd, buf, sizeof(buf))) > 0) {
                appendn(output, buf, n);
            }
            close(fd);

            write16(reply, ANS_OK);
            write32(reply, (uint32_t)output->length);
            appendn(reply, output->data, output->length);

            free_buf(output);
            break;
        }

        case OP_TERMINATE: {
            write16(reply, ANS_OK);
            char buff = 'q';
            write(status_fd, &buff ,1);
            int rep_fd = open(rep_pipe_path->data, O_WRONLY);
            if (rep_fd < 0) {
                perror("open reply pipe");
                free_buf(reply);
                return -1;
            }
            write_atomic_chunks(rep_fd, reply->data, reply->length);
            close(rep_fd);
            free_buf(reply);
            return 1;
        }

        default:
            fprintf(stderr, "Unknown opcode: 0x%04x\n", opcode);
            return -1;
    }
    int rep_fd = open(rep_pipe_path->data, O_WRONLY);
    if (rep_fd < 0) {
        perror("open reply pipe");
        free_buf(reply);
        return -1;
    }
    write_atomic_chunks(rep_fd, reply->data, reply->length);
    close(rep_fd);
    free_buf(reply);
    return 0;
}

int init_req_handler(string_t *req_pipe_path, string_t *rep_pipe_path, task_array_t *tasks, string_t *tasks_path, int status_fd){
    
    int req_fd = open(req_pipe_path->data, O_RDWR);

    if (req_fd < 0) {
        perror("open request pipe");
        return -1;
    }
    int r = 0;
    while(r == 0){
        r = handle_request(req_fd, rep_pipe_path, tasks, tasks_path, status_fd);
        if (r < 0) {
            perror("Handle_request");
            char buff = 'q';
            write(status_fd, &buff ,1);
            printf("Sent: %c\n", buff);
            close(req_fd);
            close(status_fd);
            exit(r);
        }
    }

    char buff = 'q';
    write(status_fd, &buff ,1);

    close(req_fd);
    close(status_fd);
    
    
    exit(0);
}

