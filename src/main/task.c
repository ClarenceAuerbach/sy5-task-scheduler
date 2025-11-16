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

/* Prints a string_t */
void print_string( string_t string){
    for(int i=0 ; i < (int) string.length ; i++){
        printf("%c", (string.data)[i]);
    }
    printf("\n");
}
/* Prints a task_t */
void print_task( task_t task){
    printf( "Task ID : %d\n", task.id );
    printf( "Timing : \n" );
    printBits(8, &(task.timings.minutes));
    printBits(2, &(task.timings.hours));
    printBits(1, &(task.timings.daysofweek));
    printf("Command : \n" );
    char type[2];
    memcpy(type, &(task.command->type), 2);
    printf("  type : %s\n" , type);
    printf("  nbcmds : %d \n" , task.command->nbcmds);
    printf("  argv :\n");
    
    for(int i=0 ; i < (int) task.command->args.argc ; i++){
        print_string((task.command->args.argv)[i]);
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

int extract_cmd(command_t * dest_cmd, char * dir_path) {
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
            uint32_t * argc = &(dest_cmd->args.argc);
            int read_val = read(fd, argc, 4);
            *argc = be32toh(*argc);

            dest_cmd->args.argv = (string_t *) malloc((*argc)*sizeof(string_t));
            string_t * argv = dest_cmd->args.argv;
            
            for(int i=0 ; i< (int) *argc; i++){
                read(fd, &((argv+i)->length), 4);
                
                uint32_t str_len = be32toh((argv+i)->length);
                ((argv+i)->length) = str_len;

                (argv+i)->data = (uint8_t *) malloc(str_len);
                
                read(fd, (argv+i)->data, str_len);

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
                closedir(dir);
                return -1;
            }
            close(fd);
        }

        struct stat st ;
        stat(dir_path, &st);
        if ( S_ISDIR(st.st_mode) ) {
            /*First pass we instantiate the necessary amount of memory */
            if(!count){
                int nb = count_dir_size(dir_path, 1);
                dest_cmd->cmd = (command_t *)malloc( nb * sizeof(command_t));
            } 
            
            /* We copy the current path because it will be modified in the recursion , 
            give the copy 64 more bytes for the size of the sub-dir file names (could be less)*/
            char dir_path_copy[strlen(dir_path)+ 64] ; 
            strcpy(dir_path_copy, dir_path);

            extract_cmd((dest_cmd->cmd) + count , dir_path_copy);

            count ++;
        }
        /* Truncates the la part of the path */
        int dir_path_len = strlen(dir_path); 
        dir_path[dir_path_len -(strlen(entry->d_name) + 1) ] = 0;
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
            
            char * dir_path_copy = malloc(strlen(dir_path) + 64) ; 

            strcpy(dir_path_copy, dir_path);
            dest_task->command = (command_t *) malloc(sizeof(command_t));
    
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
        int dir_path_len = strlen(dir_path); 
        
        char * dir_path_copy = malloc(dir_path_len+ 64); 
        strcpy(dir_path_copy, dir_path);
        
        task[i] = (task_t *) malloc(sizeof(task_t));
        task[i]->id = atoi(entry->d_name);
        ret += extract_task( task[i] , dir_path_copy);
        
        free(dir_path_copy);
        /* Truncates the last part of the path */
        dir_path[dir_path_len - (strlen(entry->d_name) + 1) ] = 0;

        i++;
    }
    return ret;
}



