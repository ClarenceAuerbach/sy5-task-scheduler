#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include<unistd.h>

char RUN_DIRECTORY[4096] ;

int main(int argc, char *argv[])
{

    pid_t child_pid = fork(); 
    assert(child_pid != -1);
    if( child_pid > 0 ) exit(EXIT_SUCCESS); // the parent process ends
    
    setsid(); // new processes will get the current PID (I think)
    child_pid = fork(); 
    assert(child_pid != -1);
    if( child_pid > 0 ) exit(EXIT_SUCCESS); 
    
    if( argc != 2 ) {
        printf( "Usage : use ´make run´ or pass $USER as an argument\n" );
        exit( EXIT_SUCCESS );
    }
    
    snprintf(RUN_DIRECTORY, 5+4096+13,"%s%s%s", "/tmp/", argv[1],"/erraid/tasks");

    char fpath_stdout[4096];
    char fpath_stderr[4096];
    snprintf(fpath_stdout, 4096+7,"%s%s", RUN_DIRECTORY,"/stdout");
    snprintf(fpath_stderr, 4096+7,"%s%s", RUN_DIRECTORY,"/stderr");

    int fd_out, fd_err ;
    if( (fd_out = open( fpath_stdout , O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR)) < 0 ) {
        printf( "Couldn't open fd_out\n" );
        exit( EXIT_SUCCESS );
    }
    if( (fd_err = open( fpath_stderr , O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR)) < 0) {
        printf( "Couldn't open fd_err\n" );
        exit( EXIT_SUCCESS );
    }

    // Optional: redirects stdout/stderr of the daemon
    dup2(fd_out, STDOUT_FILENO);
    dup2(fd_err, STDERR_FILENO);
    
    char * buf = "Out message from deamon\nwith error\n" ;

    while(true)
    {  
    /*  To stop the process get PID with ps aux | grep erraid and kill PID */
        
        write(fd_out, buf, 24);
        fsync(fd_out); // forces flush

        write(fd_err, buf, 35);
        fsync(fd_err); 
        
        sleep(1);
    }

    close(fd_out);
    close(fd_err);
    return 0;
}