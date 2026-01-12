#ifndef TASK_H
#define TASK_H
#define _GNU_SOURCE

#include <endian.h>
#include <stdint.h>
#include <str_util.h>
#include <timing_t.h>

/* argv[0] contains the name of the command to be called
 * it must exist and be nonempty
 */
typedef struct {
    uint32_t argc;
    string_t *argv;
} arguments_t;

/* type:
 * "SI" if simple task
 * "SQ" if sequential
 * "PL" if pipeline
 * "IF" if conditional
 *
 * If simple type, args is defined, nbcmds and cmd are not.
 * If any other (complex) type, args will not be defined, nbcmds and cmd will be.
 */

typedef struct command_t {
    char type[3];
    arguments_t args;
    uint32_t nbcmds;
    struct command_t *cmd;
} command_t;

/* Represents an entire task
 * Only represents simple or sequential commands for now
 */
typedef struct {
    uint64_t id;
    command_t *command;
    timing_t timings;
} task_t;

typedef struct {
    int length;
    task_t **tasks;
    time_t *next_times;
} task_array_t;


int find_task_index(task_array_t *tasks, uint64_t taskid) ;

void print_task(task_t task);

int extract_all(task_array_t *task_arr, string_t *dir_path);

int extract_task(task_t *dest_task, string_t *dir_path);

int extract_cmd(command_t *dest_cmd, string_t *cmd_path);

/* Free helpers */
void free_task_arr(task_array_t *task_arr);

int init_task_array(task_array_t **task_arrayp, string_t *tasks_path);

int remove_task_dir(string_t *task_dir_path);

int create_simple_task(string_t *tasks_path, uint64_t taskid, uint64_t minutes, uint32_t hours, uint8_t days, uint32_t argc, buffer_t * argv);

int create_combine_task(task_array_t * task_array, string_t *tasks_path, uint64_t taskid, uint64_t minutes, uint32_t hours, uint8_t days, uint32_t nb_task, uint64_t * task_ids, uint64_t type);
#endif
