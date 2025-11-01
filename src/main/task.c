#include <stdint.h>

struct string_t {
    uint32_t length;
    uint8_t *data;
};

/* bitmap representing what times a command should be run at
 * if the 8th and 16th hours are marked,
 * and the first (Monday) and fifth (Friday) days are marked
 * then it'll run Monday 8:00, Monday 16:00, Friday 8:00, Friday 16:00
 */
struct timing_t {
    uint64_t minutes;
    uint32_t hours;
    uint8_t daysofweek;
};

/* argv[0] contains the name of the command to be called
 * it must exist and be nonempty
 */
struct arguments_t {
    int argc;
    struct string_t *argv;
};

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
};

/* Represents an entire task
* Only represents simple or sequential commands for now
*/
struct task_t {
    int id;
    struct command_t *command;
    struct timing_t timings;
};

// TODO (not necessarily final prototype)
// int extract_task(task_t *task, char *dir_path, int id);

// TODO
// int extract_all(task_t **task, char *dir_path);
