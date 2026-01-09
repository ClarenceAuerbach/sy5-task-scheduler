#ifndef ERRAID_REQ
#define ERRAID_REQ

#include "task.h"
#include <signal.h>

// Modifies stop_requested to 1 if received command is TM - terminate
int handle_request(int req_fd, int rep_fd, task_array_t *tasks, string_t *tasks_path, volatile sig_atomic_t *stop_requested);

#endif