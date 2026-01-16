#include <assert.h>
#include <bits/types/timer_t.h>
#include <dirent.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "str_util.h"
#include "erraid_req.h"
#include "task.h"
#include "timing_t.h"

string_t *RUN_DIRECTORY;
string_t *PIPES_DIRECTORY;

static volatile sig_atomic_t stop_requested = 0;

/* Handles SIGTERM, SIGINT */
static void handle_stop(int sig) {
    (void)sig;
    stop_requested = 1;
}

void reap_zombies() {
    int status;
    while(1) {
        pid_t p = waitpid(-1, &status, WNOHANG);
        if (p > 0)  continue;
        if (p == 0) return;
        if (p == -1) {
            if (errno == EINTR) continue;
            if (errno == ECHILD) return;
            return;
        }
    }
}

/* Waits until timeout or child process message
 */
int tube_timeout(int tube_fd, int timeout) {
    if (timeout < 0) timeout = 0;

    struct pollfd p = {
        .fd = tube_fd,
        .events = POLLIN
    };
    int ret = poll(&p, 1, timeout);
    // DEBUG
    // printf("revents value : %d\n", p.revents);
    
    return ret;
}

int exec_command(command_t *com);

/* Execute a simple command - always forks */
int exec_simple_command(command_t *com) {
    if (strcmp(com->type, "SI")) return -1;
    
    int pid;
    int status;
    
    switch (pid = fork()) {
        case -1:
            perror("Error when initializing process");
            return -1;
            
        case 0: {
            /* Child process */
            char **argv = malloc((com->args.argc + 1) * sizeof(char *));
            if (!argv) {
                perror("malloc");
                _exit(1);
            }
            
            for (int i = 0; i < (int)com->args.argc; i++) {
                argv[i] = com->args.argv[i].data;
            }
            argv[com->args.argc] = NULL;

            execvp(argv[0], argv);
            
            /* Only reached if execvp fails */
            perror("execvp failed");
            free(argv);
            _exit(127);
        }
        
        default: {
            /* Parent process */
            int w;
            do {
                w = waitpid(pid, &status, 0);
            } while (w == -1 && errno == EINTR);
            
            if (w == -1) {
                perror("waitpid");
                return -1;
            }
            
            if (WIFEXITED(status)) {
                return WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                return 128 + WTERMSIG(status);
            }
            return -1;
        }
    }
}

/* Executes a sequential command (type SQ) */
int exec_sequential_command(command_t *com) {
    int ret = 0;
    for (unsigned int i = 0; i < com->nbcmds; i++) {
        ret = exec_command(&com->cmd[i]);
    }
    return ret;
}

/* Executes a conditional command (type IF) */
int exec_conditional_command(command_t *com) {
    int ret = 0;
    int cond;
    
    cond = exec_command(com->cmd);
    if (cond == 0) {
        ret = exec_command(com->cmd + 1);
    } else if (com->nbcmds >= 3) {
        ret = exec_command(com->cmd + 2);
    }
    return ret;
}

/* Executes a pipeline command (type PL) */
int exec_pipeline_command(command_t *com) {
    int n = com->nbcmds;
    int pipes[2 * (n - 1)];
    pid_t pids[n];

    /* Create all pipes */
    for (int i = 0; i < n - 1; i++) {
        if (pipe(pipes + 2*i) < 0) {
            perror("pipe");
            return -1;
        }
    }

    /* Fork children for each stage of the pipeline */
    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            /* Close all pipes on error */
            for (int j = 0; j < 2*(n-1); j++) {
                close(pipes[j]);
            }
            return -1;
        }
        
        if (pid == 0) {
            /* Child process */
            
            /* Redirect stdin from previous pipe (if not first command) */
            if (i > 0) {
                if (dup2(pipes[2*(i-1)], STDIN_FILENO) == -1) {
                    perror("dup2 stdin");
                    _exit(1);
                }
            }

            /* Redirect stdout to next pipe (if not last command) */
            if (i < n - 1) {
                if (dup2(pipes[2*i + 1], STDOUT_FILENO) == -1) {
                    perror("dup2 stdout");
                    _exit(1);
                }
            }
            /* Note: STDERR stays as is (already redirected by handle_command) */

            /* Close all pipe fds in child */
            for (int j = 0; j < 2*(n-1); j++) {
                close(pipes[j]);
            }

            /* Execute the command for this pipeline stage */
            int ret = exec_command(&com->cmd[i]);
            _exit(ret);
        }

        pids[i] = pid;
    }

    /* Parent: close all pipes */
    for (int i = 0; i < 2*(n-1); i++) {
        close(pipes[i]);
    }

    /* Wait for all children and get exit status of last command */
    int status;
    int ret = 0;
    for (int i = 0; i < n; i++) {
        pid_t w;
        do {
            w = waitpid(pids[i], &status, 0);
        } while (w == -1 && errno == EINTR);
        
        if (w == -1) {
            perror("waitpid");
            ret = -1;
        }
        
        /* Only the last command's exit status matters */
        if (i == n - 1) {
            if (WIFEXITED(status)) {
                ret = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                ret = 128 + WTERMSIG(status);
            } else {
                ret = 1;
            }
        }
    }
    
    return ret;
}

/* Main dispatch function */
int exec_command(command_t *com) {
    if (!strcmp(com->type, "SQ")) {
        return exec_sequential_command(com);
    } else if (!strcmp(com->type, "SI")) {
        return exec_simple_command(com);
    } else if (!strcmp(com->type, "IF")) {
        return exec_conditional_command(com);
    } else if (!strcmp(com->type, "PL")) {
        return exec_pipeline_command(com);
    }
    return -1;
}

/* Calls exec_command and writes time exit code */
void handle_command(command_t *com, int fd_out, int fd_err, int fd_exc){
    int pid = fork();
    if (pid > 0) return;
    if (pid == -1) {
        perror("Failed fork");
        return;
    }

    /* Redirect STDOUT and STDERR */
    if (dup2(fd_out, STDOUT_FILENO) == -1) {
        perror("dup2 stdout");
        _exit(1);
    }
    if (dup2(fd_err, STDERR_FILENO) == -1) {
        perror("dup2 stderr");
        _exit(1);
    }
    
    close(fd_out);
    close(fd_err);


    uint16_t ret = (uint16_t)exec_command(com);
    
    /* Write time and exit code */
    uint16_t be_ret = htobe16(ret);
    time_t now = time(NULL);
    uint64_t be_time = htobe64(now);
    unsigned char buf[10];
    memcpy(buf, &be_time, 8);
    memcpy(buf + 8, &be_ret, 2);

    int w = write(fd_exc, buf, sizeof(buf));
    if (w != sizeof(buf)) {
        perror("write times-exitcodes");
    }
    
    close(fd_exc);
    _exit(0);
}

/* Runs  returns -1 on error,
 * sets timeout to time until next task execution in ms, or -1 if no tasks remain
 */
int run(char *tasks_path, task_array_t *task_array, int *timeout){
    int ret = 0; 
    char *stdout_path = NULL;
    char *stderr_path = NULL;
    char *times_exitc_path = NULL;
    int fd_out = -1, fd_err = -1, fd_exc = -1;
    time_t now;
    time_t min_timing;
    size_t index;
    int found_task = 0;

    // DEBUG
    // print_task_ids(task_array->length, task_array->tasks);
    if (task_array->length == 0) goto end;

    /* allocate full PATH_MAX buffers for constructed paths to avoid
     * repeated strcat/strcpy overflows in downstream code that mutates
     * the passed path strings. Using PATH_MAX is simpler and safer here. */
    stdout_path = malloc(PATH_MAX);
    if (!stdout_path) goto cleanup;
    stderr_path = malloc(PATH_MAX);
    if (!stderr_path) goto cleanup;
    times_exitc_path = malloc(PATH_MAX);
    if (!times_exitc_path) goto cleanup;

    // DEBUG 
    // print_task(*task_array->tasks[0]);

    while(1) {
        /*Look for soonest task to be executed*/ 
        index = 0;
        found_task = 0;
        min_timing = -1;

        for (int i = 0; i < task_array->length; i++) {
            time_t t = task_array->next_times[i];
            if (t >= 0 && (!found_task || t < min_timing)) {
                index = i;
                min_timing = t;
                found_task = 1;
            }
        }
        if (!found_task) break;

        /* Check if the soonest task must be executed*/
        now = time(NULL);
        if (min_timing > now) {
            // DEBUG 
            // printf("\033[32mBreaking out of execution loop\033[0m\n");
            break; // Soonest task is still in the future.
        }

        /* Execute the task */ 
        snprintf(stdout_path, PATH_MAX, "%s/%ld/stdout", tasks_path, task_array->tasks[index]->id);
        snprintf(stderr_path, PATH_MAX, "%s/%ld/stderr", tasks_path, task_array->tasks[index]->id);
        snprintf(times_exitc_path, PATH_MAX, "%s/%ld/times-exitcodes", tasks_path, task_array->tasks[index]->id);

        fd_out = open(stdout_path, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
        if (fd_out == -1) goto cleanup;
        fd_err = open(stderr_path, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
        if (fd_err == -1) goto cleanup;
        fd_exc = open(times_exitc_path, O_WRONLY | O_CREAT | O_APPEND, S_IRWXU);
        if (fd_exc == -1) goto cleanup;

        // DEBUG
        // printf("\n\033[31mStarting execution!\033[0m\n");
        handle_command(task_array->tasks[index]->command, fd_out, fd_err, fd_exc);

        close(fd_err);
        close(fd_out);
        close(fd_exc);

        // DEBUG 
        //print_exc(times_exitc_path);

        /* clear strings */
        if (stdout_path[0]) stdout_path[0] = '\0';
        if (stderr_path[0]) stderr_path[0] = '\0';
        if (times_exitc_path[0]) times_exitc_path[0] = '\0';

        /*  Update its next_time
           (Works because its previous time was in the past, as dictated by check_time)*/
        task_array->next_times[index] = next_exec_time(task_array->tasks[index]->timings, now);
    }
    // DEBUG
    // printf("Min timing: %s", ctime(&min_timing));

end:
    if (found_task) *timeout = (min_timing-now)*1000;
    else *timeout = 86400000; // one day in ms
cleanup:
    if (fd_out >= 0) close(fd_out);
    if (fd_err >= 0) close(fd_err);
    if (fd_exc >= 0) close(fd_exc);
    if (stdout_path) free(stdout_path);
    if (stderr_path) free(stderr_path);
    if (times_exitc_path) free(times_exitc_path);
    reap_zombies();
    return ret;
}

int create_pipes(string_t *request_pipe_path, string_t *reply_pipe_path) {
    if (mkdir(PIPES_DIRECTORY->data, 0700) != 0 && errno != EEXIST) {
        perror("mkdir pipes_dir");
        return -1;
    }

    set_str(request_pipe_path, PIPES_DIRECTORY->data);
    append(request_pipe_path, "/erraid-request-pipe");
    set_str(reply_pipe_path, PIPES_DIRECTORY->data);
    append(reply_pipe_path, "/erraid-reply-pipe");
    if (mkfifo(request_pipe_path->data, 0600) != 0 && errno != EEXIST) {
        perror("mkfifo request pipe");
        return -1;
    }

    if (mkfifo(reply_pipe_path->data, 0600) != 0 && errno != EEXIST) {
        perror("mkfifo reply pipe");
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    int foreground = 0;
    int opt;

    /* Defaults */
    RUN_DIRECTORY = new_str("/tmp/");
    append(RUN_DIRECTORY, getenv("USER"));
    append(RUN_DIRECTORY, "/erraid");
    PIPES_DIRECTORY = new_str(RUN_DIRECTORY->data);
    append(PIPES_DIRECTORY, "/pipes");

    while ((opt = getopt(argc, argv, "fFr:R:p:P:")) != -1) {
        switch (opt) {
        case 'f':
        case 'F':
            foreground = 1;
            break;
        case 'r':
        case 'R':
            set_str(RUN_DIRECTORY, optarg);
            set_str(PIPES_DIRECTORY, optarg);
            append(PIPES_DIRECTORY, "/pipes");
            break;
        case 'p':
        case 'P':
            set_str(PIPES_DIRECTORY, optarg);
            break;

        default:
            fprintf(stderr,
                "Usage: erraid [-F] [-R RUN_DIR] [-P PIPES_DIR]\n");
            exit(1);
        }
    }

    /* Creates run directory if it doesn't exist */
    if (mkdir(RUN_DIRECTORY->data, 0700) != 0 && errno != EEXIST) {
        perror("mkdir RUN_DIRECTORY");
        return -1;
    }

    /*Double fork() keeping the grand-child, exists in a new session id*/
    if (!foreground) {
        pid_t child_pid = fork(); 
        assert(child_pid != -1);
        if (child_pid > 0) _exit(EXIT_SUCCESS); 
        setsid();
        child_pid = fork(); 
        assert(child_pid != -1);
        if (child_pid > 0 ) exit(EXIT_SUCCESS); 
    }
    
    /* install signal handlers to request graceful shutdown */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_stop;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    
    puts("Welcome !\nEnjoy Cran™ the task-scheduler.\n⊹ ࣪ ﹏𓊝﹏𓂁﹏⊹ ࣪ ˖");
    /* ============================================== */
    /* ========== DO NOT CHANGE THE ABOVE =========== */

    task_array_t *task_array = NULL;
    string_t *tasks_path = init_str();
    int ret = 0;
    int timeout =0;
    /* Pipe for parent<-child communication */
    int pipes_fd[2] = {-1, -1};
    /* Pipes for tadmor<->erraid communication */
    string_t *req_pipe_path = init_str();
    string_t *rep_pipe_path = init_str();


    append(tasks_path,RUN_DIRECTORY->data);
    append(tasks_path, "/tasks");

    /* creates tasks directory if non existent*/
    if (mkdir(tasks_path->data, 0700) != 0 && errno != EEXIST) {
        perror("mkdir tasks");
        goto cleanup;
    }

    if (create_pipes(req_pipe_path,rep_pipe_path) != 0) {
        perror("Failed to create pipes");
        return 1;
    }
    // DEBUG
    // printf("Pipe path: %s\n", rep_pipe_path->data);

    if (init_task_array(&task_array, tasks_path) != 0) {
        perror("init_task_array failed");
        goto cleanup;
    }

    if (pipe(pipes_fd) != 0) {
        perror("Failed to open pipes");
        goto cleanup;
    }

    /* Creates child process to check handle requests*/
    if (fork() == 0){
        close(pipes_fd[0]);
        init_req_handler(req_pipe_path, rep_pipe_path, task_array, tasks_path, pipes_fd[1]);
    }
    close(pipes_fd[1]);

    int status;
    /* ============================================== */
    /* Main loop: exit when a stop signal is received */
    
    while(!stop_requested) {
        ret = run(tasks_path->data, task_array, &timeout);
        if(ret < 0){
            perror("An Error occured during run()");
            continue;
        }
        
        status = tube_timeout(pipes_fd[0], timeout);
        if (status < 0) {
            perror("Error with poll");
            break;
        }
        if (status > 0) { // check tubes
            char buff;
            ssize_t n = read(pipes_fd[0], &buff, 1);
            
            if (n == 0) {
                printf("All writers closed the status pipe\n");
                stop_requested = 1;
                break;
            } else if (n < 0) {
                perror("read from status pipe");
                break;
            }
            switch(buff) {
                case 'q':{
                    stop_requested = 1;
                    break;
                }
                case 'c':{
                    free_task_arr(task_array);
                    task_array = NULL;
                    init_task_array(&task_array, tasks_path);
                    break;
                }
                case 'w':{
                
                    int current_timeout = 0;
                    time_t now = time(NULL);
                    time_t min_timing = -1;
                    int found_task = 0;
                    string_t * task_list = init_str();
                    char tmp_id[64];
                    
                    // Find the soonest task
                    for (int i = 0; i < task_array->length; i++) {
                        time_t t = task_array->next_times[i];
                        if (t >= 0 && (!found_task || t <= min_timing)) {
                            if (min_timing != t) trunc_str_to(task_list, 0);
                            min_timing = t;
                            snprintf(tmp_id, 64, " %ld", task_array->tasks[i]->id);
                            append(task_list, tmp_id);
                            found_task = 1;
                        }
                    }
                    
                    if (found_task && min_timing > now) {
                        current_timeout = (min_timing - now);
                        printf("Time until next task execution: %ds\nTasks ids: %s\n", current_timeout, task_list->data);
                    } else if (found_task) {
                        printf("A task should be executed now (or is overdue)\n");
                    } else {
                        printf("No tasks scheduled\n");
                    }
                    break;
                }
                default:
                    fprintf(stderr, "Unknown command from status pipe: %c (0x%02x)\n", buff, (unsigned char)buff);
                    break;               
            }
        }
    }
    /* ============================================== */
    cleanup:
        free_str(RUN_DIRECTORY);
        free_str(PIPES_DIRECTORY);
        free_str(rep_pipe_path);
        free_str(req_pipe_path);
        if( pipes_fd[0] >= 0) close(pipes_fd[0]);
        if (task_array) {
            free_task_arr(task_array);
            free(task_array);
        }
        if (tasks_path) free_str(tasks_path);
    return ret;
}
