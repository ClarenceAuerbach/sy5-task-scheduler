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

#include "task.h"
#include "timing_t.h"
#include "erraid_util.h"

char RUN_DIRECTORY[PATH_MAX];

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
		close(fd_out);
		close(fd_err);
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

    int paths_length = strlen(tasks_path) + 16;
    stdout_path = malloc(paths_length);
    if (!stdout_path) goto cleanup;
    stderr_path = malloc(paths_length);
    if (!stderr_path) goto cleanup;
    times_exitc_path = malloc(paths_length);
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
        
        // Execute the task
        sprintf(stdout_path, "%s/%d/stdout", tasks_path, task_array->tasks[index]->id);
        sprintf(stderr_path, "%s/%d/stderr", tasks_path, task_array->tasks[index]->id);
        sprintf(times_exitc_path, "%s/%d/times-exitcodes", tasks_path, task_array->tasks[index]->id);

        fd_out = open(stdout_path, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
        if (fd_out == -1) goto cleanup;
        fd_err = open(stderr_path, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
        if (fd_err == -1) goto cleanup;
        fd_exc = open(times_exitc_path, O_WRONLY | O_CREAT | O_APPEND, S_IRWXU);
        if (fd_exc == -1) goto cleanup;

        // DEBUG
        printf("\n\033[31mStarting execution!\033[0m\n");
        ret = exec_command(task_array->tasks[index]->command, fd_out, fd_err);

        time_t now = time(NULL);
        now = htobe64(now);
        uint16_t beret = htobe16(ret);
        if (write(fd_exc, &now, 8) != 8) {
            perror("Write failed");
            goto cleanup;
        }
        if (write(fd_exc, &beret, 2) != 2) {
            perror("Write failed");
            goto cleanup;
        }

        // DEBUG 
        print_exc(times_exitc_path);
        
        close(fd_exc);
        close(fd_out);
        close(fd_err);

        memset(stdout_path, 0, paths_length);
        memset(stderr_path, 0, paths_length);
        memset(times_exitc_path, 0, paths_length);

        // Update its next_time
        // (Works because its previous time was in the past, as dictated by check_time)
        task_array->next_times[index] = next_exec_time(task_array->tasks[index]->timings, now);
    }
    // DEBUG
    printf("Sleep time until next task: %lds\n", min_timing - now);
    if (min_timing - now > 0) {
        sleep(min_timing - now);
    }

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
    if (newpath == NULL || strlen(newpath)==0) {
        snprintf(RUN_DIRECTORY, PATH_MAX,"/tmp/%s/erraid",  getenv("USER"));
    } else {
        strcpy(RUN_DIRECTORY, newpath);
    } 
}

int main(int argc, char *argv[]) {
    /* Removing double fork for autotests

    // Double fork() keeping the grand-child, exists in a new session id
    pid_t child_pid = fork(); 
    assert(child_pid != -1);
    if (child_pid > 0) _exit(EXIT_SUCCESS); 
    
    setsid();
    child_pid = fork(); 
    assert(child_pid != -1);
    if (child_pid > 0 ) exit(EXIT_SUCCESS); 
    puts("");
    // DO NOT CHANGE THE ABOVE

    */
    task_array_t *task_array = NULL;
    char *tasks_path = NULL;
    
    task_array = malloc(sizeof(task_array_t));
    if (!task_array) goto cleanup;

    change_rundir((argc==2) ? argv[1] : "");

    tasks_path = malloc(strlen(RUN_DIRECTORY)+7);
    if (!tasks_path) goto cleanup;

    strcpy(tasks_path, RUN_DIRECTORY);
    strcat(tasks_path, "/tasks");

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

    /* Main loop */
    int ret = 0;
    while(1) {
        run(tasks_path, task_array);
    }
    
    // Should only exit on error
cleanup:
    if (tasks_path) free(tasks_path);
    if (task_array) {
        if (task_array->tasks) free(task_array->tasks);
        if (task_array->next_times) free(task_array->next_times);
        free(task_array);
    }
}
