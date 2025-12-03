#define _GNU_SOURCE

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

#include "task.h"
#include "erraid_util.h"

/* Command directory to struct command_t, recursive */
int extract_cmd(command_t *dest_cmd, char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        perror("cannot open dir : cmd");
        return -1;
    }

    struct dirent *entry;
    int i = 0;
    char path[PATH_MAX];
    while ((entry = readdir(dir))) {
        if( !strcmp(entry->d_name , ".") || !strcmp(entry->d_name , "..")) continue;

        /* Build a safe path into local buffer instead of mutating dir_path */
        int n = snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
        if (n < 0 || n >= (int)sizeof(path)) {
            /* truncated — skip this entry */
            continue;
        }

        if (!strcmp(entry->d_name,"argv")) {
            int fd = open(path, O_RDONLY);
            if (fd < 0) {
                closedir(dir);
                return -1;
            }
            uint32_t arc = 0;
            ssize_t read_val = read(fd, &arc, sizeof(arc));
            if (read_val != (ssize_t)sizeof(arc)) {
                close(fd);
                closedir(dir);
                return -1;
            }
            arc = be32toh(arc);

            dest_cmd->args.argc = arc;
            dest_cmd->args.argv = calloc(arc, sizeof(string_t));
            if (arc > 0 && dest_cmd->args.argv == NULL) {
                close(fd);
                closedir(dir);
                return -1;
            }
            string_t *argv = dest_cmd->args.argv;

            for(uint32_t i = 0; i < arc; i++){
                uint32_t len_be = 0;
                if (read(fd, &len_be, sizeof(len_be)) != (ssize_t)sizeof(len_be)) {
                    /* clean partial allocation */
                    for (uint32_t j = 0; j < i; j++) free(argv[j].data);
                    free(argv);
                    close(fd);
                    closedir(dir);
                    return -1;
                }
                uint32_t str_len = be32toh(len_be);
                argv[i].length = str_len;
                argv[i].data = malloc(str_len + 1); /* +1 for \0 */
                if (!argv[i].data) {
                    for (uint32_t j = 0; j < i; j++) free(argv[j].data);
                    free(argv);
                    close(fd);
                    closedir(dir);
                    return -1;
                }

                if (read(fd, argv[i].data, str_len) != (ssize_t)str_len) {
                    for (uint32_t j = 0; j <= i; j++) free(argv[j].data);
                    free(argv);
                    close(fd);
                    closedir(dir);
                    return -1;
                }
                argv[i].data[str_len] = '\0';
            }

            close(fd);
        }

        if (!strcmp(entry->d_name,"type")) {
            int fd = open(path, O_RDONLY);
            if (fd < 0) {
                closedir(dir);
                return -1;
            }
            
            if (read(fd, dest_cmd->type, 2*sizeof(char)) != 2) {
                close(fd);
                closedir(dir);
                return -1;
            }
            dest_cmd->type[2] = '\0';
            close(fd);
        }

        struct stat st ;
        if (stat(path, &st) == -1) {
            /* can't stat — skip */
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            /* First pass we instantiate the necessary amount of memory */
            if(!i){
                int nb = count_dir_size(dir_path, 1);
                dest_cmd->cmd = calloc(nb, sizeof(command_t));
            } 

            /* We copy the current path because it will be modified in the recursion,
            give the copy 64 more bytes for the size of the sub-dir file names (could be less)*/
            /* pass a duplicated path for recursion */
            char *dir_path_copy = malloc(strlen(path) + 1);
            if (!dir_path_copy) {
                closedir(dir);
                return -1;
            }
            strcpy(dir_path_copy, path);
            int idx = atoi(entry->d_name);

            extract_cmd((dest_cmd->cmd) + idx , dir_path_copy);
            free(dir_path_copy);

            i ++;
        }
    }
    dest_cmd->nbcmds = (uint32_t) i;
    closedir(dir);
    return 0;
}

/* Task directory to struct task_t, calls extract_cmd */
int extract_task(task_t *dest_task, char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        perror("cannot open dir");
        return -1;
    }

    int ret = 0;
    struct dirent *entry;
    char path[PATH_MAX];
    while ((entry = readdir(dir))) {
        if( !strcmp(entry->d_name , ".") || !strcmp(entry->d_name , "..")) continue;

        int n = snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
        if (n < 0 || n >= (int)sizeof(path)) continue;

        if (!strcmp(entry->d_name,"timing")) {
            int fd = open(path, O_RDONLY);
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
            /* We copy the current path because it will be modified in the recursion , 
            give the copy 64 more bytes for the size of the sub-dir file names (could be less)*/
            char *dir_path_copy = malloc(strlen(path) + 1);
            if (!dir_path_copy) {
                closedir(dir);
                return -1;
            }
            strcpy(dir_path_copy, path);

            dest_task->command = malloc(sizeof(command_t));
            if (!dest_task->command) {
                free(dir_path_copy);
                closedir(dir);
                return -1;
            }

            ret += extract_cmd(dest_task->command, dir_path_copy);

            free(dir_path_copy);
        }

    }

    closedir(dir);
    return ret;
}

/* Extracts all the tasks in a dir_path directory, calls extract_task */
int extract_all(task_array_t *task_arr, char *dir_path) {
    task_t **tasks = task_arr->tasks;
    
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        perror("cannot open tasks");
        return -1;
    }

    struct dirent *entry;
    int ret = 0;
    int i = 0;
    char path[PATH_MAX];
    while ((entry = readdir(dir))) {
        if( !strcmp(entry->d_name , ".") || !strcmp(entry->d_name , "..")) continue;

        int n = snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
        if (n < 0 || n >= (int)sizeof(path)) continue;

        char * dir_path_copy = malloc(strlen(path) + 1);
        if (!dir_path_copy) {
            free(dir_path_copy);
            closedir(dir);
            return -1;
        }
        strcpy(dir_path_copy, path);

        tasks[i] = malloc(sizeof(task_t));
        if (!tasks[i]) {
            free(dir_path_copy);
            free(tasks[i]);
            closedir(dir);
            return -1;
        }
        tasks[i]->id = atoi(entry->d_name);
        ret += extract_task( tasks[i] , dir_path_copy);

        free(dir_path_copy);

        i++;
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
