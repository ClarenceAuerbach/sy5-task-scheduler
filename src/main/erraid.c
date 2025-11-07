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

char RUN_DIRECTORY[4096] ;

int main(int argc, char *argv[])
{
    //signal (SIGCHLD, SIG_IGN);    how to treat zombie childs? wait() might cause a problem for example with child sleeping when demon needs to continue running

    pid_t child_pid = fork(); 
    assert(child_pid != -1);
    if( child_pid > 0 ) exit(EXIT_SUCCESS); // the parent process ends
    
    setsid(); // new processes will get the current PID (I think)
    child_pid = fork(); 
    assert(child_pid != -1);
    if( child_pid > 0 ) exit(EXIT_SUCCESS); 
    
    pid_t daemon_pid = getpid();
    
    if( argc != 2 ) {
        printf( "Usage : use ´make run´ or pass $USER as an argument\n" );
        exit( EXIT_SUCCESS );
    }
    
    snprintf(RUN_DIRECTORY, 5+4096+13,"%s%s%s", "/tmp/", argv[1],"/erraid/tasks");


    char fpath_erraid_pid[4096];
    
    snprintf(fpath_erraid_pid, 4096+15,"%s%s", RUN_DIRECTORY,"/erraid_pid.pid");

    int fd_dpid;
    if( (fd_dpid = open( fpath_erraid_pid , O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) < 0 ) {
        printf( "Couldn't open fd_out\n" );
        exit( EXIT_SUCCESS );
    }else{
        char pid[20];
        snprintf(pid, 20, "%d", daemon_pid);
        write(fd_dpid, pid, strlen(pid));
    }

    char fpath_stdout[4096];
    char fpath_stderr[4096];

    snprintf(fpath_stdout, 4096+7,"%s%s", RUN_DIRECTORY,"/stdout");
    snprintf(fpath_stderr, 4096+7,"%s%s", RUN_DIRECTORY,"/stderr");

    int fd_out, fd_err ;
    if( (fd_out = open( fpath_stdout , O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) < 0 ) {
        printf( "Couldn't open fd_out\n" );
        exit( EXIT_SUCCESS );
    }
    if( (fd_err = open( fpath_stderr , O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) < 0) {
        printf( "Couldn't open fd_err\n" );
        exit( EXIT_SUCCESS );
    }

    // redirects stdout/stderr of the daemon
    dup2(fd_out, STDOUT_FILENO);
    dup2(fd_err, STDERR_FILENO);
    
    char * buf = " Out message from deamon with error" ;

    while(true)
    {  
    /*  To stop the process get PID with ps aux | grep erraid and kill PID */
        
        time_t now = time(NULL);
        char *t = ctime(&now);
        
        write(fd_out, buf, 25);
        write(fd_out, t, strlen(t));
        fsync(fd_out); // forces flush

        write(fd_err, buf, 35);
        write(fd_err, t, strlen(t));
        fsync(fd_err); 
        
        sleep(1);
    }

    close(fd_out);
    close(fd_err);
    return 0;
}

int exe_command(command_t *com){
    
    if (com->type == 'SI'){
        switch (fork()){
        case -1: perror ("error when intializing processus");
        return -1;
        case 0: execvp(com->args.argv[0].data, com->args.argv+1);
        _exit(1);
        default: break;
        }
    }
    return 0;
}