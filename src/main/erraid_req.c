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
#include <sys/types.h>
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

int handle_request(int req_fd, string_t* rep_pipe_path, task_array_t **task_arrayp, string_t *tasks_path, int status_fd) {
    uint16_t opcode;
    buffer_t *reply = init_buf();
    int r = read16(req_fd, &opcode);
    if (r == -1) {
        perror("Reading opcode");
        free_buf(reply);
        return -1;
    }
    
    // DEBUG
    // printf("Received opcode: 0x%04x\n", opcode);
    task_array_t *task_array = *task_arrayp;
    switch(opcode) {

        case OP_COMBINE: {
            uint64_t minutes = 0;
            uint32_t hours = 0;
            uint8_t days = 0;
            uint16_t type = 0;
            uint32_t nb_task = 0;
            uint64_t taskid = 0;

            if (read64(req_fd, &minutes) < 0) {
                free_buf(reply);
                return -1;
            }
            if (read32(req_fd, &hours) < 0) {
                free_buf(reply);
                return -1;
            }
            if (read(req_fd, &days, 1) < 0) {
                free_buf(reply);
                return -1;
            }

            while ( find_task_index(task_array, taskid) != -1 ){
                taskid++;
            }

            read16(req_fd, &type);

            read32(req_fd, &nb_task);

            uint64_t task_ids[nb_task];
            for(int i =0; i< (int)nb_task; i++){
                read64(req_fd, (task_ids+i));
            }

            
            for(int i =0; i< (int)nb_task; i++){
                printf( " id recu %ld\n", task_ids[i]);
            }

            if(create_combine_task(task_array, tasks_path, taskid, minutes, hours, days, nb_task, task_ids, type) != 0) {
                write16(reply, ANS_ERROR);
                write16(reply, ERR_CANNOT_CREATE);
                break;
            }

            char buff = 'c';
            write(status_fd, &buff ,1);
            printf("Updated erraid about task creation\n");

            // Re-initialize task array
            free_task_arr(task_array);
            *task_arrayp = NULL;
            if (init_task_array(task_arrayp, tasks_path) != 0) {
                perror("Re-initializing task array");
                free_buf(reply);
                return -1;
            }
            
            write16(reply, ANS_OK);

            break;
        }

        case OP_CREATE: {
           
            uint64_t minutes = 0;
            uint32_t hours = 0;
            uint8_t days = 0;
            uint64_t taskid = 0;

            if (read64(req_fd, &minutes) < 0) {
                free_buf(reply);
                return -1;
            }
            if (read32(req_fd, &hours) < 0) {
                free_buf(reply);
                return -1;
            }
            if (read(req_fd, &days, 1) < 0) {
                free_buf(reply);
                return -1;
            }
            
            while ( find_task_index(task_array, taskid) != -1 ){
                taskid++;
            }

            uint32_t argc = 0;
            if (read32(req_fd, &argc) < 0) {
                free_buf(reply);
                return -1;
            }

            buffer_t *argv = init_buf();

            for(uint32_t i = 0; i < argc; i++){
                uint32_t len = 0;
                if (read32(req_fd, &len) < 0) {
                    free_buf(reply);
                    return -1;
                }
                write32(argv, len);
                uint8_t tmp_buff[len];

                read(req_fd, tmp_buff, len);
                appendn(argv, tmp_buff, len);
            }

            if(create_simple_task(tasks_path, taskid, minutes, hours, days, argc, argv) != 0) {
                write16(reply, ANS_ERROR);
                write16(reply, ERR_CANNOT_CREATE);
                free_buf(argv);
                break;
            }
            free_buf(argv);

            char buff = 'c';
            write(status_fd, &buff ,1);
            printf("Updated erraid about task creation\n");

            // Re-initialize task array
            free_task_arr(task_array);
            *task_arrayp = NULL;
            if (init_task_array(task_arrayp, tasks_path) != 0) {
                perror("Re-initializing task array");
                free_buf(reply);
                return -1;
            }
            
            write16(reply, ANS_OK);

            break;
        }

        case OP_REMOVE: {
            uint64_t taskid;
            if (read64(req_fd, &taskid) < 0) {
                free_buf(reply);
                return -1;
            }

            int idx = find_task_index(task_array, taskid);
            if (idx < 0) {
                // Task not found
                write16(reply, ANS_ERROR);
                write16(reply, ERR_NOT_FOUND);
                break;
            }

            string_t *task_dir_path = new_str(tasks_path->data);
            char id_path[65];
            sprintf(id_path, "/%ld", taskid);
            append(task_dir_path, id_path);
            
            if (remove_task_dir(task_dir_path) != 0) {
                free_str(task_dir_path);
                write16(reply, ANS_ERROR);
                write16(reply, ERR_NOT_FOUND);
                break;
            }
            free_str(task_dir_path);

            char buff = 'c';
            write(status_fd, &buff ,1);
            printf("Updated erraid about task removal\n");
            
            
            // Re-initialize task array
            free_task_arr(task_array);
            *task_arrayp = NULL;
            if (init_task_array(task_arrayp, tasks_path) != 0) {
                perror("Re-initializing task array");
                free_buf(reply);
                return -1;
            }
            
            write16(reply, ANS_OK);
            break;

        }
        
        case OP_LIST: {
            write16(reply, ANS_OK);
            
            write32(reply, (uint32_t)task_array->length);
            
            for (int i = 0; i < task_array->length; i++) {
                write64(reply, task_array->tasks[i]->id);
                
                // TIMING
                timing_t *t = &task_array->tasks[i]->timings;
                write64(reply, t->minutes);   // uint64 big-endian
                write32(reply, t->hours);      // uint32 big-endian
                
                appendn(reply, (uint8_t*)&t->daysofweek, 1);

                write_command(reply, task_array->tasks[i]->command);
            }
            break;
        }

        case OP_TIMES_EXITCODES: {
            uint64_t taskid;
            if (read64(req_fd, &taskid) < 0) {
                free_buf(reply);
                return -1;
            }
            
            int idx = find_task_index(task_array, taskid);
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
                    free_buf(reply);
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
            int idx = find_task_index(task_array, taskid);
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

int init_req_handler(string_t *req_pipe_path, string_t *rep_pipe_path, task_array_t *task_array, string_t *tasks_path, int status_fd){
    
    int req_fd = open(req_pipe_path->data, O_RDWR);

    if (req_fd < 0) {
        perror("open request pipe");
        return -1;
    }
    int r = 0;
    while(r == 0){
        r = handle_request(req_fd, rep_pipe_path, &task_array, tasks_path, status_fd);
        if (r < 0) {
            perror("Handle_request");
            char buff = 'q';
            write(status_fd, &buff ,1);
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

