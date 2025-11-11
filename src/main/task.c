#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>

#include "task.h"

/* Print bits for any datatype assumes little endian */
void printBits(size_t const size, void const * const ptr)
{
    unsigned char *b = (unsigned char*) ptr;
    unsigned char byte;
    int i, j;
    
    for (i = size-1; i >= 0; i--) {
        for (j = 7; j >= 0; j--) {
            byte = (b[i] >> j) & 1;
            printf("%u", byte);
        }
    }
    puts("");
}

/* Prints a task_t */
void print_task( task_t task){
    printf( "Task ID : %d\n", task.id );
    printf( "Timing : \n" );
    printBits(8, &(task.timings.minutes));
    printBits(2, &(task.timings.hours));
    printBits(1, &(task.timings.daysofweek));
    printf("Command : \n" );
    printf("  type : %d \n" , task.command->type);
    printf("  nbcmds : %d \n" , task.command->nbcmds);
    printf("  argv :\n");
    for( int i=0 ; i < task.command->args.argc ; i ++){
        char data[task.command->args.argv[i].length];
        for( int j=0 ; j < (int) task.command->args.argv[i].length; j ++){
            data[j] = (char) (task.command->args.argv[i].data)[j];
        }
        printf( "    %s \n" , data);
    }
}

/* Counts the amount of files in a dir if only_count_dir = 0 ,
*  counts the amount of directories if only_count_dir = 1 */
int count_dir_size(char *dir_path , int only_count_dir)
{
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        perror("cannot open dir path");
        return -1;
    }
    struct dirent *entry;
    int i = 0;
    while ((entry = readdir(dir))) {
        if( !strcmp(entry->d_name , ".") || !strcmp(entry->d_name , "..")) continue;

        struct stat st ;
        stat(dir_path, &st);
        if ( !only_count_dir || S_ISDIR(st.st_mode) ) i++;
    }
    return i;
}

int extract_cmd(command_t * dest_cmd, char * cmd_path) {
    DIR *dir = opendir(cmd_path);

    if (dir == NULL) {
        perror("cannot open dir : cmd");
        return -1;
    }
    struct dirent *entry;
    int count;
    while ((entry = readdir(dir))) {

        if( !strcmp(entry->d_name , ".") || !strcmp(entry->d_name , "..")) continue;

        strcat(cmd_path , "/");
        strcat(cmd_path , entry->d_name);
        printf("%s\n", cmd_path);
        if (!strcmp(entry->d_name,"argv")) {
            int fd = open(cmd_path, O_RDONLY);
            if (fd < 0) {
                closedir(dir);
                return -1;
            }
            int read_val = read(fd, &(dest_cmd->args), sizeof(arguments_t));
            //printf("%s", ((dest_cmd->args).argv)->data);
            if (read_val < 0) {
                closedir(dir);
                return -1;
            }
            close(fd);
        }

        if (!strcmp(entry->d_name,"type")) {
            int fd = open(cmd_path, O_RDONLY);
            if (fd < 0) {
                closedir(dir);
                return -1;
            }
            int read_val = read(fd, &(dest_cmd->type), sizeof(uint16_t));
            if (read_val < 0) {
                closedir(dir);
                return -1;
            }
            close(fd);
        }
        struct stat st ;
        stat(cmd_path, &st);
        if ( S_ISDIR(st.st_mode) ) {
            /*First pass we instantiate the necessary amount of memory */
            if(!count){
                int nb = count_dir_size(cmd_path, 1);
                dest_cmd->cmd = (command_t *)malloc( nb * sizeof(command_t));
            } 
            
            /* We copy the current path because it will be modified in the recursion , 
            give the copy 64 more bytes for the size of the sub-dir file names (could be less)*/
            char dir_path_copy[strlen(cmd_path)+ 64] ; 
            strcpy(dir_path_copy, cmd_path);

            extract_cmd((dest_cmd->cmd) + count , dir_path_copy);

            count ++;
        }
        /* Truncates the la part of the path */
        int dir_path_len = strlen(cmd_path); 
        cmd_path[dir_path_len -(strlen(entry->d_name) + 1) ] = 0;
    }
    dest_cmd->nbcmds = (uint32_t) count;
    return 0;
}

/* Task directory to struct task_t, calls extract_cmd */
int extract_task(task_t *dest_task, char *dir_path)
{     
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
            int read_val = read(fd, &(dest_task->timings), sizeof(timing_t));
            if (read_val < 0) {
                closedir(dir);
                return -1;
            }
            close(fd);
        }

        struct stat st ;
        stat(dir_path, &st);
        if ( S_ISDIR(st.st_mode) ) {
            /* We copy the current path because it will be modified in the recursion , 
            give the copy 64 more bytes for the size of the sub-dir file names (could be less)*/
            char dir_path_copy[strlen(dir_path) + 64] ; 
            strcpy(dir_path_copy, dir_path);
            dest_task->command = (command_t *) malloc(sizeof(command_t));
            extract_cmd(dest_task->command, dir_path_copy);
        }

        /* Truncates the la part of the path */
        int dir_path_len = strlen(dir_path); 
        dir_path[dir_path_len -(strlen(entry->d_name) + 1) ] = 0;

    }

    closedir(dir);
    return 0;
}

/* Extracts all the tasks in a dir_path directory, calls extract_task */
int extract_all(task_t *task[], char *dir_path)
{
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
        
        char dir_path_copy[strlen(dir_path) + 64] ; 
        strcpy(dir_path_copy, dir_path);
        
        task[i] = (task_t *)malloc(sizeof(task_t));
        ret += extract_task( task[i] , dir_path_copy);
        
        /* Truncates the la part of the path */
        int dir_path_len = strlen(dir_path); 
        dir_path[dir_path_len -(strlen(entry->d_name) + 1) ] = 0;

        i++;
    }
    return ret;
}



