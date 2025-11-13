#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    
    char * tasks_path = malloc(strlen(RUN_DIRECTORY)+6);
    strcpy(tasks_path,RUN_DIRECTORY);
    strcat(tasks_path, "/tasks");

    int tasks_length = count_dir_size(tasks_path , 1);
    tasks = (task_t * * ) malloc(tasks_length *sizeof(task_t *));
    printf( "\n");
    if ((ret = extract_all(tasks, tasks_path))){
        perror(" extract_all failed ");
        return -1;
    }
    print_task( *(tasks[0]));
    print_task( *(tasks[1]));

    int fd1 , fd2 , fd3 ;
    char stdout_path[strlen(tasks_path) + 16];
    char stderr_path[strlen(tasks_path) + 16];
    char times_exitc_path[strlen(tasks_path) + 16];
    for(int i=0 ; i< tasks_length ; i++){
        /*  \/ check tasks[i]->timings */ 
        if( 1 ){
            snprintf(stdout_path, strlen(stdout_path), "%s/%d/stdout", tasks_path, tasks[i]->id);
            snprintf(stderr_path, strlen(stderr_path), "%s/%d/stderr", tasks_path, tasks[i]->id);
            snprintf(times_exitc_path, strlen(times_exitc_path), "%s/%d/times-exitcodes", tasks_path, tasks[i]->id);
            printf( "%s\n" , stdout_path);
            printf( "%s\n" , stderr_path);
            printf( "%s\n" , times_exitc_path);
            fd1 = open( stdout_path, O_WRONLY | O_CREAT | O_TRUNC , S_IRWXU);
            fd2 = open( stderr_path, O_WRONLY | O_CREAT | O_TRUNC , S_IRWXU);
            fd3 = open( times_exitc_path, O_WRONLY | O_CREAT | O_TRUNC , S_IRWXU);

            dup2( fd1 , STDOUT_FILENO);
            dup2( fd2, STDERR_FILENO);
            close(fd1);
            close(fd2);
            /* TODO how to write in times exitcodes ?*/
            ret = exec_command(tasks[i]->command);

            close(fd3);
            memset( stdout_path, 0, strlen(stdout_path));
            memset( stderr_path, 0, strlen(stderr_path));
            memset( times_exitc_path, 0, strlen(times_exitc_path));
        }

    }

    sleep(60);
    return ret;
}

void change_rundir(char * newpath){
    /* Instantiating RUN_DIRECTORY with default directory */
    if(strlen(newpath)==0){
        snprintf(RUN_DIRECTORY, strlen(getenv("USER"))+19,"/tmp/%s/erraid",  getenv("USER"));
    }else{
        strcpy(RUN_DIRECTORY, newpath);
    } 
    /* Writing the pid in a file for make kill */
    char pid_path[strlen(RUN_DIRECTORY)] ;
    strcpy(pid_path, RUN_DIRECTORY);
    strcat(pid_path,"/erraid_pid.pid");
    int fd;
    char * deamon_pid = malloc(log10((double)getpid()));
    sprintf(deamon_pid, "%d", getpid());
    if( (fd = open( pid_path, O_WRONLY | O_CREAT | O_TRUNC , S_IRWXU)) ) write( fd , deamon_pid , 16);

    free(deamon_pid);
    close(fd);
}

int main(int argc, char *argv[])
{
    if( argc > 2 ) {
        printf( "Usage : use ´make run´ or pass `run directory path` as an argument\n" );
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

    change_rundir((argc==2)? argv[1]:"" );
    
    
    /* Main loop */
    int ret = 0;
    while(1)
    {  
        ret += run();
        if( ret != 0){
            perror( " An Error occured during run() ");
            break;
        }
        break;
    }
    return ret;
}

