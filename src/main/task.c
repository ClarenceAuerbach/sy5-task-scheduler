#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>

#include "task.h"

int extract_cmd(command_t * dest_cmd, char * cmd_path) {
    DIR *dir = opendir(cmd_path);

    if (dir == NULL) {
        perror("cannot open dir : cmd");
        return -1;
    }
    struct dirent *entry;
    int count;
    while ((entry = readdir(dir))) {
        char *name = entry->d_name;
        char tmp[ strlen(cmd_path)];
        strcpy(tmp, cmd_path);
        snprintf(cmd_path, strlen(cmd_path)+strlen(name) +1, "%s/%s", tmp, name);

        if (!strcmp(name,"argv")) {
            int fd = open(cmd_path, O_RDONLY);
            if (fd < 0) {
                closedir(dir);
                return -1;
            }
            int read_val = read(fd, &(dest_cmd->args), sizeof(arguments_t));
            if (read_val < (int) sizeof(arguments_t)) {
                closedir(dir);
                return -1;
            }
            close(fd);
        }

        if (!strcmp(name,"type")) {
            int fd = open(cmd_path, O_RDONLY);
            if (fd < 0) {
                closedir(dir);
                return -1;
            }
            int read_val = read(fd, &(dest_cmd->type), sizeof(uint16_t));
            if (read_val < (int) sizeof(uint16_t)) {
                closedir(dir);
                return -1;
            }
            close(fd);
        }
        struct stat st ;
        stat(cmd_path, &st);
        if ( S_ISDIR(st.st_mode) ) {
            char dir_path_tmp[strlen(cmd_path)] ; 
            strcpy(dir_path_tmp, cmd_path);
            
            extract_cmd((dest_cmd->cmd) + count , cmd_path);
            
            strcpy(cmd_path, dir_path_tmp);

            count ++;
        }
        int dir_path_len = strlen(cmd_path); /* possibly you've saved the length previously */
        cmd_path[dir_path_len -(strlen(name) + 1) ] = 0;
    }
    dest_cmd->nbcmds = (uint32_t) count;
    return 0;
}

int extract_task(task_t *dest_task, char *dir_path){
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        perror("cannot open dir");
        return -1;
    }
    struct dirent *entry;
    
    while ((entry = readdir(dir))) {
        char *name = entry->d_name;
        char tmp[ strlen(dir_path)];
        strcpy(tmp, dir_path);
        snprintf(dir_path, strlen(dir_path)+strlen(name) +1, "%s/%s", tmp, name);

        if (!strcmp(name, ".") || !strcmp(name, "..")) {
            continue;
        }

        if (!strcmp(name,"timing")) {
            int fd = open(dir_path, O_RDONLY);
            if (fd < 0) {
                closedir(dir);
                return -1;
            }
            int read_val = read(fd, &(dest_task->timings), sizeof(timing_t));
            if (read_val < (int) sizeof(timing_t)) {
                closedir(dir);
                return -1;
            }
            close(fd);
        }

        struct stat st ;
        stat(dir_path, &st);
        if ( S_ISDIR(st.st_mode) ) {
            char dir_path_tmp[strlen(dir_path)] ; 
            strcpy(dir_path_tmp, dir_path);
            
            extract_cmd(dest_task->command, dir_path);
            
            strcpy(dir_path, dir_path_tmp);
        }
        int dir_path_len = strlen(dir_path); /* possibly you've saved the length previously */
        dir_path[dir_path_len -(strlen(name) + 1) ] = 0;

    }

    closedir(dir);
    return 0;
}

int extract_all(task_t *task[], char *dir_path){
    int ret = 0;
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        perror("cannot open tasks");
        return -1;
    }
    struct dirent *entry;
    int i = 0;
    
    while ((entry = readdir(dir))) {
        char tmp[ strlen(dir_path)];
        strcpy(tmp, dir_path);
        snprintf(dir_path, strlen(dir_path)+strlen(entry->d_name) +1, "%s/%s", tmp, entry->d_name);
        ret += extract_task( task[i] , dir_path);
        i++;
    }
    return ret;
}
