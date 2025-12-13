#include <assert.h>
#include <bits/types/timer_t.h>
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "erraid_util.h"
#include "tube_util.h"
#include "task.h"
#include "timing_t.h"

char RUN_DIRECTORY[PATH_MAX];

static volatile sig_atomic_t stop_requested = 0;

static void handle_stop(int sig) {
    (void)sig;
    stop_requested = 1;
}

void reap_zombies() {
    int status;
    while(1) {
        pid_t p = waitpid(-1, &status, WNOHANG);
        if (p > 0)  continue;
        if (p == 0) return;
        if (p == -1) {
            if (errno == EINTR) continue;
            if (errno == ECHILD) return;
            return;
        }
    }
}

/* Execute a simple command of type SI */
int exec_simple_command(command_t *com, int fd_out, int fd_err){
    if (strcmp(com->type, "SI")) return -1; // Not simple
    int pid;
    int status;
    switch (pid = fork()){
        case -1:
            perror ("Error when intializing process");
            return -1;
        case 0: {
            /* allocate array of char* pointers */
            char **argv = malloc((com->args.argc + 1) * sizeof(char *));
            for (int i = 0; i < (int)com->args.argc; i++){
                argv[i] = com->args.argv[i].data;
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

int exec_command(command_t *com, int fd_out, int fd_err); // Mutually recursive

int exec_sequential_command(command_t *com, int fd_out, int fd_err){
    int ret = 0;
    for (unsigned int i=0; i < com->nbcmds; i++){
        ret = exec_command(&com->cmd[i], fd_out, fd_err);
    }
    return ret;
}

/* TODO Execute commands of every type correctly */
int exec_command(command_t *com, int fd_out, int fd_err){
    int ret = 0;
    if (!strcmp(com->type, "SQ")){
        // DEBUG
        // printf("\033[35mSequential task started\033[0m\n");
        ret = exec_sequential_command(com, fd_out, fd_err);
    } else if (!strcmp(com->type, "SI")){
        //  DEBUG
        // printf("\033[34mSimple task started\033[0m\n");
        ret = exec_simple_command(com, fd_out, fd_err);
    }
    // DEBUG
    // printf("\033[33mCommand type: %s\033[0m\n", com->type);
    return ret;
}

void handle_command(command_t *com, int fd_out, int fd_err, int fd_exc){
    int pid = fork();
    if (pid > 0) return;
    if (pid == -1) {
        perror("Failed fork");
    }

    uint16_t ret = (uint16_t)exec_command(com, fd_out, fd_err);
    uint16_t be_ret = htobe16(ret);
    time_t now = time(NULL);
    uint64_t be_time = htobe64(now);
    unsigned char buf[10];
    memcpy(buf, &be_time, 8);
    memcpy(buf + 8, &be_ret, 2);

    int w = write(fd_exc, buf, sizeof(buf));
    if (w != sizeof(buf)) {
        perror("write times-exitcodes");
    }
    _exit(0);
}

/* Waits for a tube with a timeout
 * ret > 0: tube readable
 * ret = 0: timeout
 * ret < 0: error or EINTR
 */
int tube_timeout(int tube_fd, int timeout) {
    if (timeout < 0) timeout = 0;

    struct pollfd p = {
        .fd = tube_fd,
        .events = POLL_IN
    };

    int ret = poll(&p, 1, timeout);
    return ret;
}

/* Runs every due task and return the time until next scheduled execution
 * -1 on error
 */
int run(char *tasks_path, task_array_t *task_array){
    int ret = -1;
    char *stdout_path = NULL;
    char *stderr_path = NULL;
    char *times_exitc_path = NULL;
    int fd_out = -1, fd_err = -1, fd_exc = -1;
    time_t now;
    time_t min_timing;
    size_t index;
    int found_task = 0;

    // DEBUG
    // print_task_ids(task_array->length, task_array->tasks);
    if (task_array->length == 0) goto end;

    /* allocate full PATH_MAX buffers for constructed paths to avoid
     * repeated strcat/strcpy overflows in downstream code that mutates
     * the passed path strings. Using PATH_MAX is simpler and safer here. */
    stdout_path = malloc(PATH_MAX);
    if (!stdout_path) goto cleanup;
    stderr_path = malloc(PATH_MAX);
    if (!stderr_path) goto cleanup;
    times_exitc_path = malloc(PATH_MAX);
    if (!times_exitc_path) goto cleanup;

    // DEBUG 
    // print_task(*task_array->tasks[0]);

    while(1) {
        /*Look for soonest task to be executed*/ 
        index = 0;
        found_task = 0;
        min_timing = -1;

        for (int i = 0; i < task_array->length; i++) {
            time_t t = task_array->next_times[i];
            if (t >= 0 && (!found_task || t < min_timing)) {
                index = i;
                min_timing = t;
                found_task = 1;
            }
        }
        if (!found_task) break;

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
        handle_command(task_array->tasks[index]->command, fd_out, fd_err, fd_exc);

        close(fd_err);
        close(fd_out);
        close(fd_exc);

        // DEBUG 
        //print_exc(times_exitc_path);

        /* clear strings */
        if (stdout_path[0]) stdout_path[0] = '\0';
        if (stderr_path[0]) stderr_path[0] = '\0';
        if (times_exitc_path[0]) times_exitc_path[0] = '\0';

        /*  Update its next_time
           (Works because its previous time was in the past, as dictated by check_time)*/
        task_array->next_times[index] = next_exec_time(task_array->tasks[index]->timings, now);
    }
    // DEBUG
    // printf("Min timing: %s", ctime(&min_timing));

end:
    if (!found_task) ret = 604800; // One week in seconds, somewhat arbitrary value
    ret = min_timing-now;
    printf("Time until next task execution: %d\n", ret);
cleanup:
    if (fd_out >= 0) close(fd_out);
    if (fd_err >= 0) close(fd_err);
    if (fd_exc >= 0) close(fd_exc);
    if (stdout_path) free(stdout_path);
    if (stderr_path) free(stderr_path);
    if (times_exitc_path) free(times_exitc_path);
    reap_zombies();
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

int create_pipes(char *request_pipe, char *reply_pipe) {
    char pipes_dir[PATH_MAX+7];
    snprintf(pipes_dir, PATH_MAX+7, "%s/pipes", RUN_DIRECTORY);

    // Créer le répertoire des pipes s'il n'existe pas
    if (mkdir(pipes_dir, 0700) != 0 && errno != EEXIST) {
        perror("mkdir pipes_dir");
        return -1;
    }

    // Construire les chemins complets
    snprintf(request_pipe, PATH_MAX+27, "%s/erraid-request-pipe", pipes_dir);
    snprintf(reply_pipe, PATH_MAX+25, "%s/erraid-reply-pipe", pipes_dir);

    // Créer le pipe de requête
    if (mkfifo(request_pipe, 0600) != 0 && errno != EEXIST) {
        perror("mkfifo request pipe");
        return -1;
    }

    // Créer le pipe de réponse
    if (mkfifo(reply_pipe, 0600) != 0 && errno != EEXIST) {
        perror("mkfifo reply pipe");
        return -1;
    }

    return 0;
}

int handle_request(int req_fd, int rep_fd, task_array_t tasks) {
    (void)req_fd; (void)rep_fd; (void)tasks;
    return 0;
}

int main(int argc, char *argv[]) {
    if( argc > 3 ) {
        printf("Pass at most two argument: ./erraid -[option] [parameter]\n");
        exit(0);
    }

    change_rundir(argc,argv);
    /* Creates run directory if it doesn't exist */
    if (mkdir(RUN_DIRECTORY, 0700) != 0 && errno != EEXIST) {
        perror("mkdir RUN_DIRECTORY");
        return -1;
    }

    char request_pipe[PATH_MAX+27];
    char reply_pipe[PATH_MAX+25];

    if (create_pipes(request_pipe,reply_pipe) != 0) {
        fprintf(stderr, "Failed to create pipes\n");
        return 1;
    }

    /*Double fork() keeping the grand-child, exists in a new session id*/

    pid_t child_pid = fork(); 
    assert(child_pid != -1);
    if (child_pid > 0) _exit(EXIT_SUCCESS); 

    setsid();
    child_pid = fork(); 
    assert(child_pid != -1);
    if (child_pid > 0 ) exit(EXIT_SUCCESS); 
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

    /* ============================================== */
    /* Main loop: exit when a stop signal is received */
    int req_fd = open(request_pipe, O_WRONLY | O_NONBLOCK);
    int rep_fd = open(reply_pipe, O_WRONLY | O_NONBLOCK);
    int status;
    while(!stop_requested) {
        ret = run(tasks_path, task_array);
        if(ret < 0){
            perror("An Error occured during run()");
        }
        else {
            status = tube_timeout(req_fd, ret);
            if (status < 0) perror("Error with poll");
            if (status == 0) { // timeout
                continue;
            }
            if (status > 0) { // check tubes
                handle_request(req_fd, rep_fd, task_array);
            }
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
