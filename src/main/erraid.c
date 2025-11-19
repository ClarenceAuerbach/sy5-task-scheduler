#define _DEFAULT_SOURCE

#include <assert.h>
#include <bits/types/timer_t.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
#include <sys/wait.h>

#include "task.h"
#include "timing_t.h"

#define SI (('S'<<8)|'I')
#define SQ (('S'<<8)|'Q')

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
                char **argv = malloc((com->args.argc + 1) * 4);
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
int exec_command(command_t * com, int fd_out, int fd_err){
    int ret = 0;
    if (com->type == SQ){
        for (unsigned int i=0; i<com->nbcmds; i++){
            ret = exec_command(&com->cmd[i], fd_out, fd_err);
        }
    } else if (com->type == SI){
        ret = exec_simple_command(com, fd_out, fd_err);
    }
    return ret;
}

void print_exc(char *path) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("Erreur ouverture fichier");
        return ;
    }
    while(1){
        int32_t ret;
        if (read(fd, &ret, 4) != 4) {
            close(fd);
            return ;
        }
        unsigned char buf[6];
        if (read(fd, buf, 6) != 6) {
            close(fd);
            return;
        }
        long ms = 0;
        for (int i = 0; i < 6; i++) {
            ms |= ((long)buf[i]) << (i * 8);
        }

        long seconds = ms / 1000;
        long milliseconds = ms % 1000;
        time_t time_sec = (time_t)seconds;
        struct tm *tm_info = localtime(&time_sec);
        
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
        
        printf("%d  %s.%03ld\n", ret,time_str, milliseconds);
    }    
}

/* Runs every due task and TODO sleeps until next task */
int run(char *tasks_path, task_array task_array){
    if (task_array.length == 0) return -1;

    int ret = 0;
    time_t now;
    time_t min_timing;

    int paths_length = strlen(tasks_path) + 16;
    char * stdout_path = malloc(paths_length);
    char * stderr_path = malloc(paths_length);
    char * times_exitc_path = malloc(paths_length);

    int fd_out, fd_err, fd_exc;

    while(1) {
        // Look for soonest task to be executed
        size_t index = 0;
        min_timing = task_array.next_time[0];

        for (int i = 0; i < task_array.length; i++) {
            if (task_array.next_time[i] < min_timing) {
                index = i;
                min_timing = task_array.next_time[i];
            }
        }

        // Check if the soonest task must be executed
        now = time(NULL);
        if (min_timing > now) {
            printf("\033[32mBreaking out of execution loop\033[0m\n");
            break; // Soonest task is still in the future.
        }
        

        // Execute the task
        sprintf(stdout_path, "%s/%d/stdout", tasks_path, task_array.tasks[index]->id);
        sprintf(stderr_path, "%s/%d/stderr", tasks_path, task_array.tasks[index]->id);
        sprintf(times_exitc_path, "%s/%d/times-exitcodes", tasks_path, task_array.tasks[index]->id);

        fd_out = open(stdout_path, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, S_IRWXU);
        fd_err = open(stderr_path, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, S_IRWXU);
        fd_exc = open(times_exitc_path, O_WRONLY | O_CREAT | O_APPEND , S_IRWXU);

        printf("\033[31mStarting execution!\033[0m\n");
        ret = exec_command(task_array.tasks[index]->command, fd_out, fd_err);

        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        long ms = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
        write(fd_exc, &ret, 4);
        write(fd_exc, &ms, 6);

        print_exc( times_exitc_path);
        
        close(fd_exc);
        close(fd_out);
        close(fd_err);

        memset(stdout_path, 0, strlen(stdout_path));
        memset(stderr_path, 0, strlen(stderr_path));
        memset(times_exitc_path, 0, strlen(times_exitc_path));

        // Update its next_time
        // (Works because its previous time was in the past, as dictated by check_time)
        now = time(NULL);
        task_array.next_time[index] = next_exec_time(task_array.tasks[index]->timings, now);
    }
    now = time(NULL); // making sure now is updated
    printf("Sleep time until next task: %lds\n", min_timing - now);
    sleep(min_timing - now); // Sleep by a little less than the time until next task execution

    free(stdout_path);
    free(stderr_path);
    free(times_exitc_path);
    return ret;
}

void change_rundir(char * newpath){
    /* Instantiating RUN_DIRECTORY with default directory */
    if(newpath == NULL || strlen(newpath)==0) {
        snprintf(RUN_DIRECTORY, PATH_MAX,"/tmp/%s/erraid",  getenv("USER"));
    } else {
        strcpy(RUN_DIRECTORY, newpath);
    } 
}

int main(int argc, char *argv[])
{
    // TODO must accept no arguments
    if( argc > 2 ) {
        printf( "Usage : use `make run` or pass `run_directory` as an argument\n" );
        exit(0);
    }

    /* Double fork() keeping the grand-child, exists in a new session id */
    pid_t child_pid = fork(); 
    assert(child_pid != -1);
    if( child_pid > 0 ) exit(EXIT_SUCCESS); 
    
    setsid();
    child_pid = fork(); 
    assert(child_pid != -1);
    if( child_pid > 0 ) exit(EXIT_SUCCESS); 

    change_rundir((argc==2) ? argv[1] : "");

    char *tasks_path = malloc(strlen(RUN_DIRECTORY)+6);
    strcpy(tasks_path,RUN_DIRECTORY);
    strcat(tasks_path, "/tasks");

    int ret = 0;
    task_t **tasks;
    int tasks_length = count_dir_size(tasks_path , 1);
    tasks = (task_t **) malloc(tasks_length *sizeof(task_t *));
    
    if ((ret = extract_all(tasks, tasks_path))){
        perror("Extract_all failed");
        return -1;
    }

    time_t *times = malloc(tasks_length * sizeof(time_t));
    time_t now = time(NULL);

    for(int i=0; i < tasks_length ; i++){
        times[i] = next_exec_time(tasks[i]->timings, now);
    }
    task_array task_array = {tasks_length, tasks, times};

    /* Main loop */
    while(1) {
        ret += run(tasks_path, task_array);
        if(ret != 0){
            perror("An Error occured during run()");
            break;
        }
    }
    return ret;
}
