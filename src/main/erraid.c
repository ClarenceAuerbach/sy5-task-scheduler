#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include<unistd.h>

#define RUN_DIRECTORY "../test/exemple-arborescence-1/tmp-username-erraid/tasks"

int main()
{

    pid_t child_pid = fork(); 
    assert(child_pid != -1);
    if( child_pid > 0 ) exit(EXIT_SUCCESS); // the parent process ends
    
    setsid(); // new processes will get the current PID (I think)
    child_pid = fork(); 
    assert(child_pid != -1);
    if( child_pid > 0 ) exit(EXIT_SUCCESS); 

    

    while(true)
    {  
    /*  To stop the process get PID with ps aux | grep erraid and kill PID */
        printf("Deamon process \n");
        sleep(1);
    }

    return 0;
}