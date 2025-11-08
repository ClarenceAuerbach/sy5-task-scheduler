#ifndef TASK_H
#define TASK_H
#include <stdint.h>

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


void print_command(command_t command);

void print_task(task_t task);

int extract_all(task_t *task[], char *dir_path);

int extract_task(task_t *dest_task, char *dir_path);

int extract_cmd(command_t * dest_cmd, char * cmd_path);

int count_dir_size(char *dir_path, int only_count_dir);

#endif
