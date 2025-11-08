#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "task.h"
#define SI (('S'<<8)|'I')
#define SQ (('S'<<8)|'Q')

char RUN_DIRECTORY[1024] ;


/* Execute a simple command of type SI */
int exec_simple_command(command_t *com){
    if (com->type == SI){
        switch (fork()){
            case -1:perror ("error when intializing processus");
                    return -1;
            case 0: execvp((const char *) com->args.argv[0].data, (char *const *)com->args.argv+1);
                    _exit(1);
            default: break;
        }
    }
    return 0;
}

/* TODO Execute commands of every type correctly */
int exec_command(command_t * com ){
    int ret = 0;
    if (com->type == SQ){
        for (unsigned int i=0; i<com->nbcmds; i++){
            ret += exec_simple_command(&com->cmd[i]);
        }
    } else if (com->type == SI){
        ret = exec_simple_command(com);
    }
    return ret; // à changer en fonction des valeurs de retour de exec_simple_command
}

/* Runs every due task and TODO sleeps until next task */
int run(){
    int ret = 0;

    task_t * * tasks;
    int tasks_length = count_dir_size(RUN_DIRECTORY , 0);
    tasks = (task_t * * ) malloc(tasks_length *sizeof(task_t *));
    
    if ((ret = extract_all(tasks, RUN_DIRECTORY))){
        perror(" extract_all failed ");
        return -1;
    }

    int fd1 , fd2 , fd3 ;
    char stdout_path[strlen(RUN_DIRECTORY) + 16];
    char stderr_path[strlen(RUN_DIRECTORY) + 16];
    char times_exitc_path[strlen(RUN_DIRECTORY) + 16];
    for(int i=0 ; i< tasks_length ; i++){
        /*  \/ check tasks[i]->timings */ 
        if( 1 ){
            snprintf(stdout_path, strlen(stdout_path), "%s/%d/stdout", RUN_DIRECTORY, tasks[i]->id);
            snprintf(stderr_path, strlen(stderr_path), "%s/%d/stderr", RUN_DIRECTORY, tasks[i]->id);
            snprintf(times_exitc_path, strlen(times_exitc_path), "%s/%d/times-exitcodes", RUN_DIRECTORY, tasks[i]->id);
            
            fd1 = open( stdout_path, O_WRONLY | O_CREAT | O_TRUNC , S_IRWXU);
            fd2 = open( stderr_path, O_WRONLY | O_CREAT | O_TRUNC , S_IRWXU);
            fd3 = open( times_exitc_path, O_WRONLY | O_CREAT | O_TRUNC , S_IRWXU);

            dup2( fd1 , STDOUT_FILENO);
            dup2( fd2, STDERR_FILENO);
            /* TODO how to write in times exitcodes ?*/
            ret = exec_command(tasks[i]->command);
            
            close(fd1);
            close(fd2);
            close(fd3);
            memset( stdout_path, 0, strlen(stdout_path));
            memset( stderr_path, 0, strlen(stderr_path));
            memset( times_exitc_path, 0, strlen(times_exitc_path));
        }

    }

    sleep(60);
    return ret;
}

int main(int argc, char *argv[])
{
    if( argc != 2 ) {
        printf( "Usage : use ´make run´ or pass $USER as an argument\n" );
        exit( EXIT_SUCCESS );
    }

    /* Double fork() keeping the grand-child, exists in a new session id */
    pid_t child_pid = fork(); 
    assert(child_pid != -1);
    if( child_pid > 0 ) exit(EXIT_SUCCESS); 
    
    setsid();
    child_pid = fork(); 
    assert(child_pid != -1);
    if( child_pid > 0 ) exit(EXIT_SUCCESS); 

    /* Instantiating RUN_DIRECTORY with default directory (argv[1] -> $USER) */
    snprintf(RUN_DIRECTORY, strlen(argv[1])+18,"/tmp/%s/erraid/tasks",  argv[1]);
    
    /* Writing the pid in a file for make kill */
    char pid_path[2048] ;
    int fd;
    pid_t deamon_pid = getpid();
    snprintf(pid_path, strlen(RUN_DIRECTORY)+15,"%s/erraid_pid.pid",  RUN_DIRECTORY);
    if( (fd = open( pid_path, O_WRONLY | O_CREAT | O_TRUNC , S_IRWXU)) ) write( fd , &deamon_pid , 4);
    
    /* Main loop */
    int ret = 0;
    while(true)
    {  
        ret += run();
        if( ret != 0){
            perror( " An Error occured during run() ");
        }
    }
    return ret;
}

