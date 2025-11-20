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
    int count = 0;
    while ((entry = readdir(dir))) {
        if( !strcmp(entry->d_name , ".") || !strcmp(entry->d_name , "..")) continue;

        strcat(dir_path , "/");
        strcat(dir_path , entry->d_name);

        if (!strcmp(entry->d_name,"argv")) {
            int fd = open(dir_path, O_RDONLY);
            if (fd < 0) {
                closedir(dir);
                return -1;
            }
            uint32_t *argc = &(dest_cmd->args.argc);
            int read_val = read(fd, argc, 4);
            *argc = be32toh(*argc);

            dest_cmd->args.argv = malloc((*argc) * sizeof(string_t));
            string_t *argv = dest_cmd->args.argv;

            for(uint32_t i = 0; i < *argc; i++){
                read(fd, &(argv[i].length), 4);

                uint32_t str_len = be32toh(argv[i].length);
                argv[i].length = str_len;
                argv[i].data = malloc(str_len);

                read(fd, argv[i].data, str_len);
            }
            if (read_val < 0) {
                closedir(dir);
                return -1;
            }
            close(fd);
        }

        if (!strcmp(entry->d_name,"type")) {
            int fd = open(dir_path, O_RDONLY);
            if (fd < 0) {
                closedir(dir);
                return -1;
            }
            int read_val = read(fd, &(dest_cmd->type), sizeof(uint16_t));
            if (read_val < 0) {
                close(fd);
                closedir(dir);
                return -1;
            }
            close(fd);
        }

        struct stat st ;
        stat(dir_path, &st);
        if (S_ISDIR(st.st_mode)) {
            /* First pass we instantiate the necessary amount of memory */
            if(!count){
                int nb = count_dir_size(dir_path, 1);
                dest_cmd->cmd = malloc(nb * sizeof(command_t));
            } 

            /* We copy the current path because it will be modified in the recursion,
            give the copy 64 more bytes for the size of the sub-dir file names (could be less)*/
            char dir_path_copy[strlen(dir_path) + 64];
            strcpy(dir_path_copy, dir_path);
            int i = atoi(entry->d_name);

            extract_cmd((dest_cmd->cmd) + i , dir_path_copy);

            count ++;
        }
        /* Truncates the last part of the path */
        int dir_path_len = strlen(dir_path); 
        dir_path[dir_path_len - (strlen(entry->d_name) + 1) ] = 0;
    }
    dest_cmd->nbcmds = (uint32_t)count;
    return 0;
}

/* Task directory to struct task_t, calls extract_cmd */
int extract_task(task_t *dest_task, char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        perror("cannot open dir");
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if( !strcmp(entry->d_name , ".") || !strcmp(entry->d_name , "..")) continue;

        strcat(dir_path , "/");
        strcat(dir_path , entry->d_name);

        if (!strcmp(entry->d_name,"timing")) {
            int fd = open(dir_path, O_RDONLY);
            if (fd < 0) {
                closedir(dir);
                return -1;
            }
            char timings[13];
            int read_val = read(fd, timings, 13);
            if (read_val < 13) {
                closedir(dir);
                return -1;
            }
            uint64_t *min;
            uint32_t *hours;
            uint8_t *days;
            min = &((dest_task->timings).minutes);
            hours = &((dest_task->timings).hours);
            days = &((dest_task->timings).daysofweek);
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
            
            char *dir_path_copy = malloc(strlen(dir_path) + 64) ; 

            strcpy(dir_path_copy, dir_path);
            dest_task->command = malloc(sizeof(command_t));

            extract_cmd(dest_task->command, dir_path_copy);

            free(dir_path_copy);
        }

        /* Truncates the la part of the path */
        int dir_path_len = strlen(dir_path); 
        dir_path[dir_path_len -(strlen(entry->d_name) + 1) ] = 0;

    }

    closedir(dir);
    return 0;
}

/* Extracts all the tasks in a dir_path directory, calls extract_task */
int extract_all(task_array_t *task_arr, char *dir_path) {
    task_t **tasks = task_arr->tasks;
    int ret = 0;
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        perror("cannot open tasks");
        return -1;
    }
    struct dirent *entry;
    int i = 0;
    while ((entry = readdir(dir))) {
        if( !strcmp(entry->d_name , ".") || !strcmp(entry->d_name , "..")) continue;
        
        strcat(dir_path , "/");
        strcat(dir_path , entry->d_name);
        int dir_path_len = strlen(dir_path); 
        
        char * dir_path_copy = malloc(dir_path_len+ 64); 
        strcpy(dir_path_copy, dir_path);
        
        tasks[i] = malloc(sizeof(task_t));
        tasks[i]->id = atoi(entry->d_name);
        ret += extract_task( tasks[i] , dir_path_copy);
        
        free(dir_path_copy);
        /* Truncates the last part of the path */
        dir_path[dir_path_len - (strlen(entry->d_name) + 1) ] = 0;

        i++;
    }
    return ret;
}


/* Free functions */

void free_cmd(command_t cmd) {
    // Command is simple, subcommands cmd and command count nbcmds are empty
    if (cmd.type == SI) {
        for (uint32_t i = 0; i < cmd.args.argc; i++) {
            free(cmd.args.argv[i].data);
        }
        free(cmd.args.argv);
    } else { // Complex command, args is empty
        for (uint32_t i = 0; i < cmd.nbcmds; i++) {
            free_cmd(cmd.cmd[i]);
        }
    }
}

void free_task(task_t *task) {
    free(task->command);
}

void free_task_arr(task_array_t *task_arr) {
    for (int i = 0; i < task_arr->length; i++) {
        free_task((task_arr->tasks)[i]);
    }
}
