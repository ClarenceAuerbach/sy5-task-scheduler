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
#include <sys/wait.h>
#include "task.h"
#include "timing_t.h"
#define SI (('S'<<8)|'I')
#define SQ (('S'<<8)|'Q')

char RUN_DIRECTORY[1024] ;

/* Execute a simple command of type SI */
int exec_simple_command(command_t *com, int fd_out, int fd_err){
    if (com->type == SI){
        int pid;
        int status;
        switch (pid = fork()){
            case -1:
                perror ("error when intializing processus");
                return -1;
            case 0: 
            {
                char **argv = malloc((com->args.argc + 1) * sizeof(char *));
                for (int i = 0; i < (int) com->args.argc; i++){
                    argv[i] = (char *)com->args.argv[i].data;
                }
                argv[com->args.argc] = NULL; 

                dup2( fd_out , STDOUT_FILENO);
                close(fd_out);
                dup2( fd_err, STDERR_FILENO);
                close(fd_err);

                printf( "something\n" );
                execvp(argv[0], argv);

                perror("execvp failed");
                _exit(127);
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
            ret += exec_simple_command(&com->cmd[i], fd_out, fd_err);
        }
    } else if (com->type == SI){
        ret = exec_simple_command(com, fd_out, fd_err);
    }
    return ret;
}

/* Runs every due task and TODO sleeps until next task */
int run(char * tasks_path, task_array task_array){
    int ret = 0;

    task_t * * tasks = task_array.tasks ; 
    int tasks_length = task_array.length;

    int paths_length = strlen(tasks_path) + 16;
    char * stdout_path = malloc(paths_length);
    char * stderr_path = malloc(paths_length);
    char * times_exitc_path = malloc(paths_length);
    int fd_out, fd_err, fd_exc;


    time_t now ;
    time(&now);
    struct tm * timeinfo = localtime (&now);
    
    for(int i=0 ; i< tasks_length ; i++){
        if( check_time( (task_array.next_time)[i], 10) ){

            printf( "something\n" );
            sprintf(stdout_path, "%s/%d/stdout", tasks_path, tasks[i]->id);
            sprintf(stderr_path, "%s/%d/stderr", tasks_path, tasks[i]->id);
            sprintf(times_exitc_path, "%s/%d/times-exitcodes", tasks_path, tasks[i]->id);

            fd_out = open( stdout_path, O_WRONLY | O_CREAT | O_TRUNC , S_IRWXU);
            fd_err = open( stderr_path, O_WRONLY | O_CREAT | O_TRUNC , S_IRWXU);
            fd_exc = open( times_exitc_path, O_WRONLY | O_CREAT | O_APPEND , S_IRWXU);

            
            ret = exec_command(tasks[i]->command, fd_out, fd_err);

            write(fd_exc, &ret, 4);

            write(fd_exc, &now, 4);
            write(fd_exc, &(timeinfo->tm_sec), 4);
            write(fd_exc, &(timeinfo->tm_sec), 2);

            close(fd_exc);
            close(fd_out);
            close(fd_err);

            memset( stdout_path, 0, strlen(stdout_path));
            memset( stderr_path, 0, strlen(stderr_path));
            memset( times_exitc_path, 0, strlen(times_exitc_path));

            (task_array.next_time)[i] = next_exec_time(tasks[i]->timings, time(NULL));
            
        }

    }

    printf("%ld\n", now - min(task_array.next_time, task_array.length) );
    sleep(min(task_array.next_time, task_array.length) );
    free(stdout_path);
    free(stderr_path);
    free(times_exitc_path);
    return ret;
}

void change_rundir(char * newpath){
    /* Instantiating RUN_DIRECTORY with default directory */
    if(strlen(newpath)==0){
        snprintf(RUN_DIRECTORY, strlen(getenv("USER"))+19,"/tmp/%s/erraid",  getenv("USER"));
    }else{
        strcpy(RUN_DIRECTORY, newpath);
    } 
}

int main(int argc, char *argv[])
{
    if( argc > 2 ) {
        printf( "Usage : use ´make run´ or pass `run directory path` as an argument\n" );
        exit( EXIT_SUCCESS );
    }

    /* Double fork() keeping the grand-child, exists in a new session id */
/*
    pid_t child_pid = fork(); 
    assert(child_pid != -1);
    if( child_pid > 0 ) exit(EXIT_SUCCESS); 
    
    setsid();
    child_pid = fork(); 
    assert(child_pid != -1);
    if( child_pid > 0 ) exit(EXIT_SUCCESS); 
*/
    
    
    change_rundir((argc==2)? argv[1]:"" );

    char * tasks_path = malloc(strlen(RUN_DIRECTORY)+6);
    strcpy(tasks_path,RUN_DIRECTORY);
    strcat(tasks_path, "/tasks");

    int ret = 0;
    task_t * * tasks;
    int tasks_length = count_dir_size(tasks_path , 1);
    tasks = (task_t * * ) malloc(tasks_length *sizeof(task_t *));
    
    if ((ret = extract_all(tasks, tasks_path))){
        perror(" extract_all failed ");
        return -1;
    }

    time_t * times = malloc(tasks_length * sizeof(time_t));
    time_t now = time(NULL);

    for(int i=0; i < tasks_length ; i++){
        times[i] = next_exec_time( tasks[i]->timings, now);
    }
    task_array task_array = { tasks_length, tasks, times};

    /* Main loop */
    while(1)
    {  
        ret += run( tasks_path, task_array);
        if( ret != 0){
            perror( " An Error occured during run() ");
            break;
        }
    }
    return ret;
}

