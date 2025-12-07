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
#include <errno.h>

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
    if (!strcmp(com->type, "SI")){
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

                    // DEBUG 
                    // printf("argv[%d] = %s\n", i, argv[i]);
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
            default: {
                int w;
                do {
                    w = waitpid(pid, &status, 0);
                } while (w == -1 && errno == EINTR);
                if (w == -1) {
                    perror("waitpid");
                    return -1;
                }
                if (WIFEXITED(status)) {
                    return WEXITSTATUS(status);  // normal exitcode
                } else if (WIFSIGNALED(status)) {
                    return 128 + WTERMSIG(status);  // Killed by a signal 
                }
                return -1;
            }
        }
    }
    return 0;
}

/* TODO Execute commands of every type correctly */
int exec_command(command_t *com, int fd_out, int fd_err){
    int ret = 0;
    if (!strcmp(com->type, "SQ")){
        // DEBUG
        // printf("\033[35mSequential task started\033[0m\n");
        for (unsigned int i=0; i < com->nbcmds; i++){
            ret = exec_command(&com->cmd[i], fd_out, fd_err);
        }
    } else if (!strcmp(com->type, "SI")){
        //  DEBUG
        // printf("\033[34mSimple task started\033[0m\n");
        ret = exec_simple_command(com, fd_out, fd_err);
    }
    // DEBUG
    // printf("\033[33mCommand type: %s\033[0m\n", com->type);
    return ret;
}

/* Runs every due task and sleeps until next task */
int run(char *tasks_path, task_array_t * task_array){
    int ret = 0;
    char *stdout_path = NULL;
    char *stderr_path = NULL;
    char *times_exitc_path = NULL;
    int fd_out = -1, fd_err = -1, fd_exc = -1;

    if (task_array->length == 0) return -1;

    time_t now;
    time_t min_timing;
    size_t index ;

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
        /*Look for soonest task to be executed*/ 
        index = 0;
        min_timing = INT_MAX;

        for (int i = 0; i < task_array->length; i++) {
            if ( task_array->next_times[i] >= 0 && task_array->next_times[i] < min_timing) {
                index = i;
                min_timing = task_array->next_times[i];
            }
        }

        /* Check if the soonest task must be executed*/
        now = time(NULL);
        if (min_timing > now) {
            // DEBUG 
            //printf("\033[32mBreaking out of execution loop\033[0m\n");
            break; // Soonest task is still in the future.
        }
        
        /* Execute the task */ 
        snprintf(stdout_path, PATH_MAX, "%s/%d/stdout", tasks_path, task_array->tasks[index]->id);
        snprintf(stderr_path, PATH_MAX, "%s/%d/stderr", tasks_path, task_array->tasks[index]->id);
        snprintf(times_exitc_path, PATH_MAX, "%s/%d/times-exitcodes", tasks_path, task_array->tasks[index]->id);

        fd_out = open(stdout_path, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
        if (fd_out == -1) goto cleanup;
        fd_err = open(stderr_path, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
        if (fd_err == -1) goto cleanup;
        fd_exc = open(times_exitc_path, O_WRONLY | O_CREAT | O_APPEND, S_IRWXU);
        if (fd_exc == -1) goto cleanup;

        // DEBUG
        // printf("\n\033[31mStarting execution!\033[0m\n");
        ret = exec_command(task_array->tasks[index]->command, fd_out, fd_err);


        now = time(NULL);
        time_t be_time = htobe64(now);
        uint16_t be_ret = htobe16(ret);

        if (write(fd_exc, &be_time, 8) != 8) {
            perror("Write failed");
            goto cleanup;
        }
        if (write(fd_exc, &be_ret, 2) != 2) {
            perror("Write failed");
            goto cleanup;
        }
        close(fd_err);
		close(fd_out);
        close(fd_exc);

        // DEBUG 
        // print_exc(times_exitc_path);
        
        /* clear strings */
        if (stdout_path[0]) stdout_path[0] = '\0';
        if (stderr_path[0]) stderr_path[0] = '\0';
        if (times_exitc_path[0]) times_exitc_path[0] = '\0';

        /*  Update its next_time
           (Works because its previous time was in the past, as dictated by check_time)*/
        task_array->next_times[index] = next_exec_time(task_array->tasks[index]->timings, now);
    }
    // DEBUG
    // printf("Min timing: %s\n", ctime(&min_timing));
    printf("Sleep time until next task: %lds\n", min_timing - now);
    if (min_timing - now > 0) {
        sleep(min_timing - now);
    }

    goto cleanup;
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
void change_rundir(int argc, char *argv[]){
    if (argc != 3) {
        snprintf(RUN_DIRECTORY, PATH_MAX,"/tmp/%s/erraid",  getenv("USER"));
    } else if(!strcmp(argv[1],"-r")){
        /* copy safely into fixed-size RUN_DIRECTORY */
        snprintf(RUN_DIRECTORY, PATH_MAX, "%s", argv[2]);
    } 
}

int main(int argc, char *argv[]) {
    if( argc > 3 ) {
        printf("Pass at most two argument: ./erraid -[option] [parameter]\n");
        exit(0);
    }

    change_rundir(argc,argv);

    /*Double fork() keeping the grand-child, exists in a new session id*/
    /*
    pid_t child_pid = fork(); 
    assert(child_pid != -1);
    if (child_pid > 0) _exit(EXIT_SUCCESS); 
    
    setsid();
    child_pid = fork(); 
    assert(child_pid != -1);
    if (child_pid > 0 ) exit(EXIT_SUCCESS); 
    puts("");
    */
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
    int ret = 0;
    int task_count = 0;
    
    task_array = malloc(sizeof(task_array_t));
    if (!task_array) goto cleanup;

    tasks_path = malloc(PATH_MAX+7);
    if (!tasks_path) goto cleanup;

    snprintf(tasks_path, PATH_MAX+7, "%s/tasks", RUN_DIRECTORY);

    task_count = count_dir_size(tasks_path , 1);
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
    return ret;
}
