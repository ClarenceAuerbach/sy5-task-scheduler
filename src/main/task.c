#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>

#define SI (('S'<<8)|'I')

typedef struct {
    uint32_t length;
    uint8_t *data;
} string_t;

/* bitmap representing what times a command should be run at
 * if the 8th and 16th hours are marked,
 * and the first (Monday) and fifth (Friday) days are marked
 * then it'll run Monday 8:00, Monday 16:00, Friday 8:00, Friday 16:00
 */
typedef struct {
    uint64_t minutes;
    uint32_t hours;
    uint8_t daysofweek;
} timing_t;

/* argv[0] contains the name of the command to be called
 * it must exist and be nonempty
 */
typedef struct {
    int argc;
    string_t *argv;
} arguments_t;

/* type:
* 'SI' if simple task
* 'SQ' if sequential
* 'PL' if pipeline
* 'IF' if conditional
*
* If simple type, args is defined, nbcmds and cmd are not.
* If any other (complex) type, args will not be defined, nbcmds and cmd will be.
*/
typedef struct command_t {
    uint16_t type;
    arguments_t args;
    uint32_t nbcmds;
    struct command_t *cmd;
} command_t;

/* Represents an entire task
* Only represents simple or sequential commands for now
*/
typedef struct {
    int id;
    command_t *command;
    timing_t timings;
} task_t;


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
