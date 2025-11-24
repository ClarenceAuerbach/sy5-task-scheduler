#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <timing_t.h>

typedef struct {
    uint32_t length;
    uint8_t *data;
} string_t;

/* argv[0] contains the name of the command to be called
 * it must exist and be nonempty
 */
typedef struct {
    uint32_t argc;
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
#define SI (('I'<<8)|'S')
#define SQ (('Q'<<8)|'S')
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

typedef struct {
    int length;
    task_t **tasks;
    time_t *next_times;
} task_array_t;

void print_task(task_t task);

int extract_all(task_array_t *task_arr, char *dir_path);

int extract_task(task_t *dest_task, char *dir_path);

int extract_cmd(command_t * dest_cmd, char * cmd_path);

int count_dir_size(char *dir_path, int only_count_dir);

/* Free helpers */
void free_task_arr(task_array_t *task_arr);

#endif
