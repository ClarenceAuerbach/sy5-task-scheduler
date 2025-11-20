#define _DEFAULT_SOURCE

#include <fcntl.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <endian.h>
#include <string.h>
#include <dirent.h>

#include "task.h"
#include "timing_t.h"
#include "erraid_util.h"

/* Prints times_exitcodes content */
void print_exc(char *path) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("Erreur ouverture fichier");
        return ;
    }
    while(1){
        time_t time = 0;
        if (read(fd, &time, 8) != 8) {
            close(fd);
            return;
        }
        time = be64toh(time);
        uint16_t ret;
        if (read(fd, &ret, 2) != 2) {
            close(fd);
            return ;
        }
        ret = be16toh(ret);

        printf("%.24s | %d\n", ctime(&time), ret);
    }
}

/* Print bits for any datatype assumes little endian */
void printBits(size_t const size, void const * const ptr){
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
void print_task(task_t task){
    printf( "Task ID : %d\n", task.id );
    printf( "Timing : \n" );
    printBits(8, &(task.timings.minutes));
    printBits(2, &(task.timings.hours));
    printBits(1, &(task.timings.daysofweek));
    printf("Command : \n" );
    char type[3];
    memcpy(type, &(task.command->type), 2);
    type[2] = '\0';
    printf("  type : %s\n", type);
    printf("  nbcmds : %d \n", task.command->nbcmds);
    printf("  argv :\n");

    for(int i=0 ; i < (int) task.command->args.argc ; i++){
        print_string((task.command->args.argv)[i]);
    }
}

/* Counts the amount of files in a dir if only_count_dir = 0 ,
*  counts the amount of directories if only_count_dir = 1 */
int count_dir_size(char *dir_path , int only_count_dir) {
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
