#define _DEFAULT_SOURCE

#include <assert.h>
#include <bits/types/timer_t.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>
#include <endian.h>
#include <signal.h>

#include "task.h"
#include "timing_t.h"
#include "erraid_util.h"

char RUN_DIRECTORY[PATH_MAX];

static volatile sig_atomic_t stop_requested = 0;

static void handle_stop(int sig) {
    (void)sig;
    stop_requested = 1;
}

/* Execute a simple command of type SI */
int exec_simple_command(command_t *com, int fd_out, int fd_err){
    if (com->type == SI){
        int pid;
        int status;
        switch (pid = fork()){
            case -1:
                perror ("Error when intializing process");
                return -1;
            case 0: {
                /* allocate array of char* pointers */
                char **argv = malloc((com->args.argc + 1) * sizeof(char *));
                for (int i = 0; i < (int) com->args.argc; i++){
                    argv[i] = malloc((com->args.argv[i].length + 1) * sizeof(char));
                    memcpy(argv[i], com->args.argv[i].data, com->args.argv[i].length);
                    argv[i][com->args.argv[i].length] = '\0';
                }
                argv[com->args.argc] = NULL; 
                
                dup2(fd_out, STDOUT_FILENO);
                close(fd_out);
                dup2(fd_err, STDERR_FILENO);
                close(fd_err);
                
                execvp(argv[0], argv);

                perror("execvp failed");
                exit(127);
            }
            default: 
                waitpid(pid, &status, 0) ; 
                if (WIFEXITED(status)) {
                    return WEXITSTATUS(status);  // normal exitcode
                } else if (WIFSIGNALED(status)) {
                    return 128 + WTERMSIG(status);  // Killed by a signal 
                }
                return -1;
        }
    }
    return 0;
}

/* TODO Execute commands of every type correctly */
int exec_command(command_t *com, int fd_out, int fd_err){
    int ret = 0;
    if (com->type == SQ){
        printf("\033[35mSequential task started\033[0m\n");
        for (unsigned int i=0; i < com->nbcmds; i++){
            ret = exec_command(&com->cmd[i], fd_out, fd_err);
        }
    } else if (com->type == SI){
        ret = exec_simple_command(com, fd_out, fd_err);
    }
    return ret;
}

/* Runs every due task and sleeps until next task */
int run(char *tasks_path, task_array_t * task_array){
    int ret = -1;
    char *stdout_path = NULL;
    char *stderr_path = NULL;
    char *times_exitc_path = NULL;
    int fd_out = -1, fd_err = -1, fd_exc = -1;

    if (task_array->length == 0) return -1;

    time_t now;
    time_t min_timing;

    /* allocate full PATH_MAX buffers for constructed paths to avoid
     * repeated strcat/strcpy overflows in downstream code that mutates
     * the passed path strings. Using PATH_MAX is simpler and safer here. */
    stdout_path = malloc(PATH_MAX);
    if (!stdout_path) goto cleanup;
    stderr_path = malloc(PATH_MAX);
    if (!stderr_path) goto cleanup;
    times_exitc_path = malloc(PATH_MAX);
    if (!times_exitc_path) goto cleanup;
    
    // DEBUG print_task(*task_array->tasks[0]);

    while(1) {
        // Look for soonest task to be executed
        size_t index = 0;
        min_timing = INT_MAX;

        for (int i = 0; i < task_array->length; i++) {
            if ( task_array->next_times[i] >= 0 && task_array->next_times[i] < min_timing) {
                index = i;
                min_timing = task_array->next_times[i];
            }
        }

        // Check if the soonest task must be executed
        now = time(NULL);
        if (min_timing > now) {
            // DEBUG
            printf("\033[32mBreaking out of execution loop\033[0m\n");
            break; // Soonest task is still in the future.
        }
        
        sleep(3);
        // Execute the task
    snprintf(stdout_path, PATH_MAX, "%s/%d/stdout", tasks_path, task_array->tasks[index]->id);
    snprintf(stderr_path, PATH_MAX, "%s/%d/stderr", tasks_path, task_array->tasks[index]->id);
    snprintf(times_exitc_path, PATH_MAX, "%s/%d/times-exitcodes", tasks_path, task_array->tasks[index]->id);

        fd_out = open(stdout_path, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, S_IRWXU);
        if (fd_out == -1) goto cleanup;
        fd_err = open(stderr_path, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, S_IRWXU);
        if (fd_err == -1) goto cleanup;
        fd_exc = open(times_exitc_path, O_WRONLY | O_CREAT | O_APPEND , S_IRWXU);
        if (fd_exc == -1) goto cleanup;

        // DEBUG
        printf("\n\033[31mStarting execution!\033[0m\n");
        ret = exec_command(task_array->tasks[index]->command, fd_out, fd_err);

        time_t now = time(NULL);
        uint64_t be_now = htobe64((uint64_t)now);
        uint16_t beret = htobe16(ret);
        if(write(fd_exc, &be_now, sizeof(be_now)) != (ssize_t)sizeof(be_now)) {
            printf("Write failed");
            goto cleanup;
        }
        if(write(fd_exc, &beret, sizeof(beret)) != (ssize_t)sizeof(beret)) {
            printf("Write failed");
            goto cleanup;
        }

        // DEBUG 
        print_exc(times_exitc_path);
        
        close(fd_exc);
        close(fd_out);
        close(fd_err);

    /* clear strings efficiently */
    if (stdout_path[0]) stdout_path[0] = '\0';
    if (stderr_path[0]) stderr_path[0] = '\0';
    if (times_exitc_path[0]) times_exitc_path[0] = '\0';

        // Update its next_time
        // (Works because its previous time was in the past, as dictated by check_time)
        now = time(NULL);
        task_array->next_times[index] = next_exec_time(task_array->tasks[index]->timings, now);
    }
    now = time(NULL); // making sure now is updated
    // DEBUG
    printf("Sleep time until next task: %lds\n", min_timing - now);
    sleep(min_timing - now); // Sleep by a little less than the time until next task execution

    ret = 0;
cleanup:
    if (fd_out >= 0) close(fd_out);
    if (fd_err >= 0) close(fd_err);
    if (fd_exc >= 0) close(fd_exc);
    if (stdout_path) free(stdout_path);
    if (stderr_path) free(stderr_path);
    if (times_exitc_path) free(times_exitc_path);
    return ret;
}

/* Instantiating RUN_DIRECTORY with default directory */
void change_rundir(char * newpath){
    if(newpath == NULL || strlen(newpath)==0) {
        snprintf(RUN_DIRECTORY, PATH_MAX,"/tmp/%s/erraid",  getenv("USER"));
    } else {
        /* copy safely into fixed-size RUN_DIRECTORY */
        snprintf(RUN_DIRECTORY, PATH_MAX, "%s", newpath);
    } 
}

int main(int argc, char *argv[]) {
    if( argc > 3 ) {
        printf("Pass at most one argument: run_directory\n");
        exit(0);
    }

    /* Double fork() keeping the grand-child, exists in a new session id */
    pid_t child_pid = fork(); 
    assert(child_pid != -1);
    if(child_pid > 0) exit(EXIT_SUCCESS); 
    
    setsid();
    child_pid = fork(); 
    assert(child_pid != -1);
    if( child_pid > 0 ) exit(EXIT_SUCCESS); 
    puts("");

    /* install signal handlers to request graceful shutdown */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_stop;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    
    /* DO NOT CHANGE THE ABOVE */
    task_array_t *task_array = NULL;
    char *tasks_path = NULL;
    
    task_array = malloc(sizeof(task_array_t));
    if (!task_array) goto cleanup;

    change_rundir((argc>=2) ? argv[1] : "");

    tasks_path = malloc(PATH_MAX+7);
    if (!tasks_path) goto cleanup;

    snprintf(tasks_path, PATH_MAX+7, "%s/tasks", RUN_DIRECTORY);

    int task_count = count_dir_size(tasks_path , 1);
    task_array->length = task_count;
    
    task_array->tasks = malloc(task_count * sizeof(task_t *));
    if (!task_array->tasks) goto cleanup;

    if (extract_all(task_array, tasks_path)){
        perror("Extract_all failed");
        goto cleanup;
    }

    task_array->next_times = malloc(task_count * sizeof(time_t));
    if (!task_array->next_times) goto cleanup;

    time_t now = time(NULL);
    for(int i = 0; i < task_count; i++) {
        task_array->next_times[i] = next_exec_time(task_array->tasks[i]->timings, now);
    }

    /* Main loop: exit when a stop signal is received */
    int ret = 0;
    while(!stop_requested) {
        ret += run(tasks_path, task_array);
        if(ret != 0){
            perror("An Error occured during run()");
            goto cleanup;
        }
    }
    
    // Should only exit on error
cleanup:
    /* Perform full cleanup of task structures */
    if (task_array) {
        free_task_arr(task_array);
        free(task_array);
    }
    if (tasks_path) free(tasks_path);
}
