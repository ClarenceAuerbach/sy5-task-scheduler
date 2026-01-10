#ifndef ERRAID_REQ
#define ERRAID_REQ

#include "str_util.h"
#include "task.h"

// Modifies stop_requested to 1 if received command is TM - terminate
int init_req_handler(string_t* req_pipe_path, string_t* rep_pipe_path, task_array_t *tasks, string_t *tasks_path, int status_fd);

int tube_timeout(int tube_fd, int timeout);
#endif