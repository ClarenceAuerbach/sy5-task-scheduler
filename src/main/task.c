#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>
#include <endian.h>

#include "tube_util.h"
#include "task.h"
#include "erraid_util.h"
#include "tube_util.h"

int extract_cmd(command_t *dest_cmd, string_t *dir_path) {
    memset(dest_cmd, 0, sizeof(*dest_cmd));

    DIR *dir = opendir(dir_path->data);
    if (!dir) {
        perror("opendir cmd");
        goto error;
    }

    uint32_t nbcmds = count_dir_size(dir_path->data, 1);

    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;

        size_t base_length = dir_path->length;

        append(dir_path, "/");
        append(dir_path, entry->d_name);

        struct stat st;
        if (stat(dir_path->data, &st) == -1)
            goto error_restore;

        /* ----- type ----- */
        if (!strcmp(entry->d_name, "type")) {
            int fd = open(dir_path->data, O_RDONLY);
            if (fd < 0) goto error_restore;

            if (read(fd, dest_cmd->type, 2) != 2) {
                close(fd);
                goto error_restore;
            }
            dest_cmd->type[2] = '\0';
            close(fd);
            goto restore;
        }

        /* ----- argv ----- */
        if (!strcmp(entry->d_name, "argv")) {
            int fd = open(dir_path->data, O_RDONLY);
            if (fd < 0) goto error_restore;

            if(read32(fd, &dest_cmd->args.argc) == -1) {
                close(fd);
                goto error_restore;
            }
            uint32_t argc = dest_cmd->args.argc;
            dest_cmd->args.argv = calloc(argc, sizeof(string_t));
            if (argc && !dest_cmd->args.argv)
                goto error_restore;

            for (uint32_t i = 0; i < argc; i++) {
                string_t *arg = &dest_cmd->args.argv[i];

                if (read32(fd, &arg->length) == -1)
                    goto error_restore;

                arg->data = malloc(arg->length + 1);

                if (!arg->data)
                    goto error_restore;

                if (read(fd, arg->data, arg->length) != arg->length)
                    goto error_restore;

                arg->data[arg->length] = '\0';
            }

            close(fd);
            goto restore;
        }

        /* ---------- subcommand directory ---------- */
        if (S_ISDIR(st.st_mode)) {
            int subcommand_id = atoi(entry->d_name);
            if (subcommand_id < 0)
                goto error_restore;

            if(!dest_cmd->cmd){ // Haven't seen a subcommand yet
                dest_cmd->nbcmds = nbcmds;
                dest_cmd->cmd = calloc(dest_cmd->nbcmds, sizeof(command_t));
            }


            if ((uint32_t)subcommand_id >= dest_cmd->nbcmds)
                goto error_restore;
            if (extract_cmd(&dest_cmd->cmd[subcommand_id], dir_path) != 0)
                goto error_restore;
        }

restore:
        trunc_str_to(dir_path, base_length);
        continue;

error_restore:
        trunc_str_to(dir_path, base_length);
        goto error;
    }

    closedir(dir);
    return 0;

error:
    if (dir) closedir(dir);
    return -1;
}

/* Task directory to struct task_t, calls extract_cmd */
int extract_task(task_t *dest_task, string_t *dir_path) {
    DIR *dir = opendir(dir_path->data);
    if (dir == NULL) {
        perror("cannot open dir");
        return -1;
    }

    int ret = 0;
    struct dirent *entry;
    while ((entry = readdir(dir))) {
        uint32_t base_length = dir_path->length;
        if( !strcmp(entry->d_name , ".") || !strcmp(entry->d_name , "..")) continue;
        
        append(dir_path, "/");
        append(dir_path, entry->d_name);

        if (!strcmp(entry->d_name,"timing")) {
            int fd = open(dir_path->data, O_RDONLY);
            if (fd < 0) {
                closedir(dir);
                return -1;
            }
            char timings[13];
            int read_val = read(fd, timings, 13);
            if (read_val < 13) {
                close(fd);
                closedir(dir);
                return -1;
            }
            uint64_t *min = &((dest_task->timings).minutes);
            uint32_t *hours = &((dest_task->timings).hours);
            uint8_t *days = &((dest_task->timings).daysofweek);
            memcpy(min, timings,  8);
            memcpy(hours, timings+8,  4);
            memcpy(days, timings+12,  1);

            *min = be64toh(*min);
            *hours = be32toh(*hours);
            close(fd);
        }

        if (!strcmp(entry->d_name,"cmd")) {
            dest_task->command = malloc(sizeof(command_t));
            if (!dest_task->command) {
                closedir(dir);
                return -1;
            }

            ret += extract_cmd(dest_task->command, dir_path);
        }
        trunc_str_to(dir_path, base_length);
    }

    closedir(dir);
    return ret;
}

/* Extracts all the tasks in a dir_path directory, calls extract_task */
int extract_all(task_array_t *task_arr, string_t *dir_path) {
    task_t **tasks = task_arr->tasks;
    
    DIR *dir = opendir(dir_path->data);
    if (dir == NULL) {
        perror("cannot open tasks");
        return -1;
    }

    struct dirent *entry;
    int ret = 0;
    int i = 0;
    while ((entry = readdir(dir))) {
        if(!strcmp(entry->d_name , ".") || !strcmp(entry->d_name , "..")) continue;
        uint32_t base_length = dir_path->length;
        append(dir_path, "/");
        append(dir_path, entry->d_name);

        tasks[i] = malloc(sizeof(task_t));
        if (!tasks[i]) {
            free(dir_path);
            free(tasks[i]);
            closedir(dir);
            return -1;
        }
        tasks[i]->id = atoi(entry->d_name);
        ret += extract_task(tasks[i] , dir_path);
        i++;
        trunc_str_to(dir_path, base_length);
    }
    closedir(dir);
    return ret;
}


/* Free functions */

void free_cmd(command_t *cmd) {
    if (!cmd) return;
    /* If it's a simple command, free argv strings and the argv array */
    if (!strcmp(cmd->type, "SI")) {
        if (cmd->args.argv) {
            for (uint32_t i = 0; i < cmd->args.argc; i++) {
                if (cmd->args.argv[i].data) {
                    free(cmd->args.argv[i].data);
                    cmd->args.argv[i].data = NULL;
                }
            }
            free(cmd->args.argv);
            cmd->args.argv = NULL;
            cmd->args.argc = 0;
        }
    } else { /* Complex command: free child commands */
        if (cmd->cmd) {
            for (uint32_t i = 0; i < cmd->nbcmds; i++) {
                free_cmd(&cmd->cmd[i]);
            }
            free(cmd->cmd);
            cmd->cmd = NULL;
            cmd->nbcmds = 0;
        }
    }
}

void free_task(task_t *task) {
    if (!task) return;
    if (task->command) {
        free_cmd(task->command);
        free(task->command);
        task->command = NULL;
    }
    free(task);
}

void free_task_arr(task_array_t *task_arr) {
    if (!task_arr) return;
    for (int i = 0; i < task_arr->length; i++) {
        if (task_arr->tasks[i]) {
            free_task(task_arr->tasks[i]);
            task_arr->tasks[i] = NULL;
        }
    }
    /* free the tasks array and timing data if present */
    if (task_arr->tasks) {
        free(task_arr->tasks);
        task_arr->tasks = NULL;
    }
    if (task_arr->next_times) {
        free(task_arr->next_times);
        task_arr->next_times = NULL;
    }
}

/* We create task_array */
int init_task_array(task_array_t **task_arrayp, string_t *tasks_path) {
    *task_arrayp = malloc(sizeof(task_array_t));
    if (!(*task_arrayp)) return -1;
    task_array_t *task_array = (*task_arrayp);
    
    int task_count = count_dir_size(tasks_path->data, 1);
    task_array->length = task_count;
    /* If there are no tasks we skip extraction*/
    if (task_count > 0) {
        task_array->tasks = malloc(task_count * sizeof(task_t *));
        if (!task_array->tasks) return -1;

        if (extract_all(task_array, tasks_path)) {
            perror("Extract_all failed");
            return -1;
        }

        task_array->next_times = malloc(task_count * sizeof(time_t));
        if (!task_array->next_times) return -1;

        time_t now = time(NULL);
        for(int i = 0; i < task_count; i++) {
            task_array->next_times[i] = next_exec_time(task_array->tasks[i]->timings, now);
        }
    } else {
        task_array->tasks = NULL;
        task_array->next_times = NULL;
    }
    return 0;
}

int remove_task_dir(string_t *task_dir_path) {
    DIR *dir = opendir(task_dir_path->data);
    if (!dir) {
        perror("opendir");
        return -1;
    }
    
    struct dirent *entry;
    int ret = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        string_t *entry_path = new_str(task_dir_path->data);
        append(entry_path, "/");
        append(entry_path, entry->d_name);
        
        struct stat statbuf;
        if (stat(entry_path->data, &statbuf) == -1) {
            perror("stat");
            free_str(entry_path);
            ret = -1;
            continue;
        }
        
        if (S_ISDIR(statbuf.st_mode)) {
            if (remove_task_dir(entry_path) != 0) {
                ret = -1;
            }
        } else {
            if (unlink(entry_path->data) != 0) {
                perror("unlink");
                ret = -1;
            }
        }
        
        free_str(entry_path);
    }
    
    closedir(dir);
    
    if (rmdir(task_dir_path->data) != 0) {
        perror("rmdir");
        return -1;
    }
    
    return ret;
}

int create_simple_task(string_t *tasks_path, uint64_t taskid, uint64_t minutes, uint32_t hours, uint8_t days, uint32_t argc, buffer_t *argv) {
    string_t *task_dir_path = new_str(tasks_path->data);
    
    char tmp[65];
    snprintf(tmp, 65, "/%lu", taskid);
    append(task_dir_path, tmp);
    
    if (mkdir(task_dir_path->data, 0700) != 0) {
        perror("mkdir task dir");
        free_str(task_dir_path);
        return -1;
    }
    
    // Create timing file
    append(task_dir_path, "/timing");
    int timing_fd = open(task_dir_path->data, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (timing_fd < 0) {
        perror("open timing file");
        free_str(task_dir_path);
        return -1;
    }

    buffer_t *timing_buf = init_buf();
    write64(timing_buf, minutes) ;
    write32(timing_buf, hours) ;
    appendn(timing_buf, &days, 1) ;

    if (write(timing_fd, timing_buf->data, timing_buf->length) < 0) {
        perror("write timing file");
        free_buf(timing_buf);
        free_str(task_dir_path);
        close(timing_fd);
        return -1;
    }
    free_buf(timing_buf);
    close(timing_fd);
    
    // Create cmd directory

    trunc_str_by(task_dir_path, 7); // remove /timing
    append(task_dir_path, "/cmd");
    if (mkdir(task_dir_path->data, 0700) != 0) {
        perror("mkdir cmd dir");
        free_str(task_dir_path);
        return -1;
    }
    
    // Create type file
    
    append(task_dir_path,  "/type");
    int type_fd = open(task_dir_path->data, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (type_fd < 0) {
        perror("open type file");
        free_str(task_dir_path);
        return -1;
    }
    
    if (write(type_fd, "SI", 2) != 2) {
        perror("write type file");
        close(type_fd);
        free_str(task_dir_path);
        return -1;
    }
    
    close(type_fd);
    
    // Create argv file

    trunc_str_by(task_dir_path, 5); // remove /type
    append(task_dir_path, "/argv");
    int argv_fd = open(task_dir_path->data, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (argv_fd < 0) {
        perror("open argv file");
        free_str(task_dir_path);
        return -1;
    }

    buffer_t* argc_buff = init_buf();
    write32(argc_buff, argc);
    if(write(argv_fd, argc_buff->data, argc_buff->length) < 0){
        perror("write argv file");
        free_str(task_dir_path);
        free_buf(argc_buff);
        close(argv_fd);
        return -1;
    }

    if (write(argv_fd, argv->data, argv->length) < 0) {
        perror("write argv file");
        free_str(task_dir_path);
        close(argv_fd);
        return -1;
    }
    char ch = '\0';
    appendn(argv, &ch, 1);
    printf("Wrote %s in argv\n", argv->data);
    free_str(task_dir_path);
    close(argv_fd);
    return 0;
}

