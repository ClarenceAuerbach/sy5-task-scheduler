#define _DEFAULT_SOURCE

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <limits.h>
#include <string.h>
#include <errno.h>

struct string_t {
    uint32_t length;
    uint8_t *data;
}typedef string_t;

/* bitmap representing what times a command should be run at
 * if the 8th and 16th hours are marked,
 * and the first (Monday) and fifth (Friday) days are marked
 * then it'll run Monday 8:00, Monday 16:00, Friday 8:00, Friday 16:00
 */
struct timing_t {
    uint64_t minutes;
    uint32_t hours;
    uint8_t daysofweek;
}typedef timing_t;

/* argv[0] contains the name of the command to be called
 * it must exist and be nonempty
 */
struct arguments_t {
    int argc;
    struct string_t *argv;
}typedef arguments_t;

/* type:
* 'SI' if simple task
* 'SQ' if sequential
* 'PL' if pipeline
* 'IF' if conditional
*
* If simple type, args is defined, nbcmds and cmd are not.
* If any other (complex) type, args will not be defined, nbcmds and cmd will be.
*/
struct command_t {
    uint16_t type;
    struct arguments_t args;
    uint32_t nbcmds;
    struct command_t *cmd;
}typedef command_t;

/* Represents an entire task
* Only represents simple or sequential commands for now
*/
struct task_t {
    int id;
    struct command_t *command;
    struct timing_t timings;
}typedef task_t;

// TODO (not necessarily final prototype)
int extract_task( char *dir_path, int id, task_t *dest_task){
    int count = 0;
    snprintf(dir_path, strlen(dir_path)+5, "%s%d", "/", id);
    
    string_append(dir_path, id);
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        perror("cannot open dir");
        goto error;
    }

    struct dirent *entry;

    while ((entry = readdir(dir))) {
        char *name = entry->d_name;

        if (!strcmp(name, ".") || !strcmp(name, "..")) {
            continue;
        }
        if (!strcmp(name,"stderr")) {
            //TODO
            count++;
        }
        if (!strcmp(name,"stdout")) {
            //TODO
            count++;
        }
        if (!strcmp(name,"times-exitcodes")) {
            //TODO
            count++;
        }
        if (!strcmp(name,"timing")) {
            //TODO
            count++;
        }
        if (!strcmp(name,"argv")) {
            //TODO
            count++;
        }
        if (!strcmp(name,"type")) {
            //TODO
            count++;
        }
        
        if (entry->d_type == DT_DIR) {
            string_append(dir_path, "/");
            string_append(dir_path, name);
            int res = process_dir(dir_path, id, dest_task);
            if (res == -1) {
                goto error;
            } else {
                count += res;
            }
        }
        string_truncate(dir_path, strlen(name)+1);
    }
    closedir(dir);
    return count;

    error:
        if (errno) perror("process_dir"); 
        if (dir) closedir(dir);
        return -1;
}

// TODO
// int extract_all(task_t *task[], char *dir_path);
