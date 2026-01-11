#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>
#include <endian.h>

#include "task.h"
#include "erraid_util.h"
#include "tube_util.h"

/* Command directory to struct command_t, recursive */
int extract_cmd(command_t *dest_cmd, char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {  
        perror("cannot open dir : cmd");
        return -1;
    }

    struct dirent *entry;
    int i = 0;
    char path[PATH_MAX];
    while ((entry = readdir(dir))) {
        if( !strcmp(entry->d_name , ".") || !strcmp(entry->d_name , "..")) continue;  

        /* Build a safe path into local buffer instead of mutating dir_path */  
        int n = snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
        if (n < 0 || n >= (int)sizeof(path)) {
            /* truncated — skip this entry */
            continue;
        }

        if (!strcmp(entry->d_name,"argv")) {
            int fd = open(path, O_RDONLY);
            if (fd < 0) {
                closedir(dir);
                return -1;
            }
            uint32_t arc = 0;
            ssize_t read_val = read(fd, &arc, sizeof(arc));
            if (read_val != (ssize_t)sizeof(arc)) {
                close(fd);
                closedir(dir);
                return -1;
            }
            arc = be32toh(arc);

            dest_cmd->args.argc = arc;
            dest_cmd->args.argv = calloc(arc, sizeof(string_t));
            if (arc > 0 && dest_cmd->args.argv == NULL) {
                close(fd);
                closedir(dir);
                return -1;
            }
            string_t *argv = dest_cmd->args.argv;

            for(uint32_t i = 0; i < arc; i++){
                uint32_t len_be = 0;
                if (read(fd, &len_be, sizeof(len_be)) != (ssize_t)sizeof(len_be)) {
                    /* clean partial allocation */
                    for (uint32_t j = 0; j < i; j++) free(argv[j].data);
                    free(argv);
                    close(fd);
                    closedir(dir);
                    return -1;
                }
                uint32_t str_len = be32toh(len_be);
                argv[i].length = str_len;
                argv[i].data = malloc(str_len + 1); /* +1 for \0 */
                if (!argv[i].data) {
                    for (uint32_t j = 0; j < i; j++) free(argv[j].data);
                    free(argv);
                    close(fd);
                    closedir(dir);
                    return -1;
                }

                if (read(fd, argv[i].data, str_len) != (ssize_t)str_len) {
                    for (uint32_t j = 0; j <= i; j++) free(argv[j].data);
                    free(argv);
                    close(fd);
                    closedir(dir);
                    return -1;
                }
                argv[i].data[str_len] = '\0';
            }

            close(fd);
        }

        if (!strcmp(entry->d_name,"type")) {
            int fd = open(path, O_RDONLY);
            if (fd < 0) {
                closedir(dir);
                return -1;
            }
            
            if (read(fd, dest_cmd->type, 2*sizeof(char)) != 2) {
                close(fd);
                closedir(dir);
                return -1;
            }
            dest_cmd->type[2] = '\0';
            close(fd);
        }

        struct stat st ;
        if (stat(path, &st) == -1) {
            /* can't stat — skip */
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            /* First pass we instantiate the necessary amount of memory */
            if(!i){
                int nb = count_dir_size(dir_path, 1);
                dest_cmd->cmd = calloc(nb, sizeof(command_t));
            } 

            /* We copy the current path because it will be modified in the recursion,
            give the copy 64 more bytes for the size of the sub-dir file names (could be less)*/
            /* pass a duplicated path for recursion */
            char *dir_path_copy = malloc(strlen(path) + 1);
            if (!dir_path_copy) {
                closedir(dir);
                return -1;
            }
            strcpy(dir_path_copy, path);
            int idx = atoi(entry->d_name);

            extract_cmd((dest_cmd->cmd) + idx , dir_path_copy);
            free(dir_path_copy);

            i ++;
        }
    }
    dest_cmd->nbcmds = (uint32_t) i;
    closedir(dir);
    return 0;
}

/* Task directory to struct task_t, calls extract_cmd */
int extract_task(task_t *dest_task, char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        perror("cannot open dir");
        return -1;
    }

    int ret = 0;
    struct dirent *entry;
    char path[PATH_MAX];
    while ((entry = readdir(dir))) {
        if( !strcmp(entry->d_name , ".") || !strcmp(entry->d_name , "..")) continue;

        int n = snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
        if (n < 0 || n >= (int)sizeof(path)) continue;

        if (!strcmp(entry->d_name,"timing")) {
            int fd = open(path, O_RDONLY);
            if (fd < 0) {
                closedir(dir);
                return -1;
            }
            char timings[13];
            int read_val = read(fd, timings, 13);
            if (read_val < 13) {
                close(fd);
                closedir(dir);
                return -1;
            }
            uint64_t *min = &((dest_task->timings).minutes);
            uint32_t *hours = &((dest_task->timings).hours);
            uint8_t *days = &((dest_task->timings).daysofweek);
            memcpy(min, timings,  8);
            memcpy(hours, timings+8,  4);
            memcpy(days, timings+12,  1);

            *min = be64toh(*min);
            *hours = be32toh(*hours);
            
            close(fd);
        }

        if (!strcmp(entry->d_name,"cmd")) {
            /* We copy the current path because it will be modified in the recursion , 
            give the copy 64 more bytes for the size of the sub-dir file names (could be less)*/
            char *dir_path_copy = malloc(strlen(path) + 1);
            if (!dir_path_copy) {
                closedir(dir);
                return -1;
            }
            strcpy(dir_path_copy, path);

            dest_task->command = malloc(sizeof(command_t));
            if (!dest_task->command) {
                free(dir_path_copy);
                closedir(dir);
                return -1;
            }

            ret += extract_cmd(dest_task->command, dir_path_copy);

            free(dir_path_copy);
        }

    }

    closedir(dir);
    return ret;
}

/* Extracts all the tasks in a dir_path directory, calls extract_task */
int extract_all(task_array_t *task_arr, char *dir_path) {
    task_t **tasks = task_arr->tasks;
    
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        perror("cannot open tasks");
        return -1;
    }

    struct dirent *entry;
    int ret = 0;
    int i = 0;
    char path[PATH_MAX];
    while ((entry = readdir(dir))) {
        if( !strcmp(entry->d_name , ".") || !strcmp(entry->d_name , "..")) continue;

        int n = snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
        if (n < 0 || n >= (int)sizeof(path)) continue;

        char * dir_path_copy = malloc(strlen(path) + 1);
        if (!dir_path_copy) {
            free(dir_path_copy);
            closedir(dir);
            return -1;
        }
        strcpy(dir_path_copy, path);

        tasks[i] = malloc(sizeof(task_t));
        if (!tasks[i]) {
            free(dir_path_copy);
            free(tasks[i]);
            closedir(dir);
            return -1;
        }
        tasks[i]->id = atoi(entry->d_name);
        ret += extract_task( tasks[i] , dir_path_copy);

        free(dir_path_copy);

        i++;
    }
    closedir(dir);
    return ret;
}


/* Free functions */

void free_cmd(command_t *cmd) {
    if (!cmd) return;
    /* If it's a simple command, free argv strings and the argv array */
    if (!strcmp(cmd->type, "SI")) {
        if (cmd->args.argv) {
            for (uint32_t i = 0; i < cmd->args.argc; i++) {
                if (cmd->args.argv[i].data) {
                    free(cmd->args.argv[i].data);
                    cmd->args.argv[i].data = NULL;
                }
            }
            free(cmd->args.argv);
            cmd->args.argv = NULL;
            cmd->args.argc = 0;
        }
    } else { /* Complex command: free child commands */
        if (cmd->cmd) {
            for (uint32_t i = 0; i < cmd->nbcmds; i++) {
                free_cmd(&cmd->cmd[i]);
            }
            free(cmd->cmd);
            cmd->cmd = NULL;
            cmd->nbcmds = 0;
        }
    }
}

void free_task(task_t *task) {
    if (!task) return;
    if (task->command) {
        free_cmd(task->command);
        free(task->command);
        task->command = NULL;
    }
    free(task);
}

void free_task_arr(task_array_t *task_arr) {
    if (!task_arr) return;
    for (int i = 0; i < task_arr->length; i++) {
        if (task_arr->tasks[i]) {
            free_task(task_arr->tasks[i]);
            task_arr->tasks[i] = NULL;
        }
    }
    /* free the tasks array and timing data if present */
    if (task_arr->tasks) {
        free(task_arr->tasks);
        task_arr->tasks = NULL;
    }
    if (task_arr->next_times) {
        free(task_arr->next_times);
        task_arr->next_times = NULL;
    }
}

/* We create task_array */
int init_task_array(task_array_t **task_arrayp, string_t *tasks_path) {
    *task_arrayp = malloc(sizeof(task_array_t));
    if (!(*task_arrayp)) return -1;
    task_array_t *task_array = (*task_arrayp);
    
    int task_count = count_dir_size(tasks_path->data, 1);
    task_array->length = task_count;
    /* If there are no tasks we skip extraction*/
    if (task_count > 0) {
        task_array->tasks = malloc(task_count * sizeof(task_t *));
        if (!task_array->tasks) return -1;

        if (extract_all(task_array, tasks_path->data)) {
            perror("Extract_all failed");
            return -1;
        }

        task_array->next_times = malloc(task_count * sizeof(time_t));
        if (!task_array->next_times) return -1;

        time_t now = time(NULL);
        for(int i = 0; i < task_count; i++) {
            task_array->next_times[i] = next_exec_time(task_array->tasks[i]->timings, now);
        }
    } else {
        task_array->tasks = NULL;
        task_array->next_times = NULL;
    }
    return 0;
}

int remove_task_dir(string_t *task_dir_path) {
    DIR *dir = opendir(task_dir_path->data);
    if (!dir) {
        perror("opendir");
        return -1;
    }
    
    struct dirent *entry;
    int ret = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        string_t *entry_path = new_str(task_dir_path->data);
        append(entry_path, "/");
        append(entry_path, entry->d_name);
        
        struct stat statbuf;
        if (stat(entry_path->data, &statbuf) == -1) {
            perror("stat");
            free_str(entry_path);
            ret = -1;
            continue;
        }
        
        if (S_ISDIR(statbuf.st_mode)) {
            if (remove_task_dir(entry_path) != 0) {
                ret = -1;
            }
        } else {
            if (unlink(entry_path->data) != 0) {
                perror("unlink");
                ret = -1;
            }
        }
        
        free_str(entry_path);
    }
    
    closedir(dir);
    
    if (rmdir(task_dir_path->data) != 0) {
        perror("rmdir");
        return -1;
    }
    
    return ret;
}

int create_simple_task(string_t *tasks_path, uint64_t taskid, uint64_t minutes, uint32_t hours, uint8_t days, uint32_t argc, buffer_t *argv) {
    string_t *task_dir_path = new_str(tasks_path->data);
    
    char tmp[65];
    snprintf(tmp, 65, "/%lu", taskid);
    append(task_dir_path, tmp);
    
    if (mkdir(task_dir_path->data, 0700) != 0) {
        perror("mkdir task dir");
        free_str(task_dir_path);
        return -1;
    }
    
    // Create timing file
    append(task_dir_path, "/timing");
    int timing_fd = open(task_dir_path->data, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (timing_fd < 0) {
        perror("open timing file");
        free_str(task_dir_path);
        return -1;
    }

    buffer_t *timing_buf = init_buf();
    write64(timing_buf, minutes) ;
    write32(timing_buf, hours) ;
    appendn(timing_buf, &days, 1) ;

    if (write(timing_fd, timing_buf->data, timing_buf->length) < 0) {
        perror("write timing file");
        free_buf(timing_buf);
        free_str(task_dir_path);
        close(timing_fd);
        return -1;
    }
    free_buf(timing_buf);
    close(timing_fd);
    
    // Create cmd directory

    trunc_str_by(task_dir_path, 7); // remove /timing
    append(task_dir_path, "/cmd");
    if (mkdir(task_dir_path->data, 0700) != 0) {
        perror("mkdir cmd dir");
        free_str(task_dir_path);
        return -1;
    }
    
    // Create type file
    
    append(task_dir_path,  "/type");
    int type_fd = open(task_dir_path->data, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (type_fd < 0) {
        perror("open type file");
        free_str(task_dir_path);
        return -1;
    }
    
    if (write(type_fd, "SI", 2) != 2) {
        perror("write type file");
        close(type_fd);
        free_str(task_dir_path);
        return -1;
    }
    
    close(type_fd);
    
    // Create argv file

    trunc_str_by(task_dir_path, 5); // remove /type
    append(task_dir_path, "/argv");
    int argv_fd = open(task_dir_path->data, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (argv_fd < 0) {
        perror("open argv file");
        free_str(task_dir_path);
        return -1;
    }

    buffer_t* argc_buff = init_buf();
    write32(argc_buff, argc);
    if(write(argv_fd, argc_buff->data, argc_buff->length) < 0){
        perror("write argv file");
        free_str(task_dir_path);
        free_buf(argc_buff);
        close(argv_fd);
        return -1;
    }

    if (write(argv_fd, argv->data, argv->length) < 0) {
        perror("write argv file");
        free_str(task_dir_path);
        close(argv_fd);
        return -1;
    }
    char ch = '\0';
    appendn(argv, &ch, 1);
    printf("Wrote %s in argv\n", argv->data);
    free_str(task_dir_path);
    close(argv_fd);
    return 0;
}


int create_command_rec( command_t * command, string_t * cmd_path){
    append(cmd_path, "/type");
    int type_fd = open(cmd_path->data, O_WRONLY |O_CREAT , 0600 );
    trunc_str_by(cmd_path, 5);
    
    if( !strcmp(command->type,"SI")){
        if (write(type_fd, "SI", 2) != 2){
            close(type_fd);
            return -1;
        }
        close(type_fd);
        buffer_t * argv_buff = init_buf();

        appendn(argv_buff, &command->args.argc, 4);
        for( int i= 0; i < (int) command->args.argc; i++ ){
            
            appendn(argv_buff, command->args.argv->data, command->args.argv->length);
        }
        append(cmd_path, "/argv");
        int argv_fd = open(cmd_path->data, O_WRONLY |O_CREAT , 0600 );

        if( write_atomic_chunks( argv_fd, argv_buff->data, argv_buff->length) < 0){
            free_str(cmd_path);
            free_buf(argv_buff);
            close(argv_fd);
            return -1;
        }
    }else {
        if (write(type_fd, command->type, 2) != 2){
                close(type_fd);
                return -1;
        }
        close(type_fd);

        int tmp_length = cmd_path->length;
        char tmp_i[16];
        for(int i = 0; i < (int)command->nbcmds ; i++){
            snprintf(tmp_i, 16, "/%d", i);
            append(cmd_path, tmp_i);
            if(mkdir( cmd_path->data, 0700) != 0){
                perror("Failed mkdir in creating command");
                return -1;
            }
            create_command_rec( &command->cmd[i], cmd_path);
            trunc_str_to(cmd_path, tmp_length);
        }
    }
    return 0;
}


int create_combine_task(task_array_t * task_array, string_t *task_dir_path, uint64_t taskid, uint64_t minutes, uint32_t hours, uint8_t days, uint32_t nb_task, uint64_t * task_ids, uint64_t type){
    
    int old_length = task_dir_path->length;
    char tmp[65];
    snprintf(tmp, 65, "/%lu", taskid);
    append(task_dir_path, tmp);
    
    if (mkdir(task_dir_path->data, 0700) != 0) {
        perror("mkdir task dir");
        free_str(task_dir_path);
        return -1;
    }
    
    // Create timing file
    append(task_dir_path, "/timing");
    int timing_fd = open(task_dir_path->data, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (timing_fd < 0) {
        perror("open timing file");
        free_str(task_dir_path);
        return -1;
    }

    buffer_t *timing_buf = init_buf();
    write64(timing_buf, minutes) ;
    write32(timing_buf, hours) ;
    appendn(timing_buf, &days, 1) ;

    if (write(timing_fd, timing_buf->data, timing_buf->length) < 0) {
        perror("write timing file");
        free_buf(timing_buf);
        free_str(task_dir_path);
        close(timing_fd);
        return -1;
    }
    free_buf(timing_buf);
    close(timing_fd);
    
    // Create cmd directory

    trunc_str_by(task_dir_path, 7); // remove /timing
    append(task_dir_path, "/cmd");
    if (mkdir(task_dir_path->data, 0700) != 0) {
        perror("mkdir cmd dir");
        free_str(task_dir_path);
        return -1;
    }
    
    // Create type file
    
    append(task_dir_path,  "/type");
    int type_fd = open(task_dir_path->data, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (type_fd < 0) {
        perror("open type file");
        free_str(task_dir_path);
        return -1;
    }

    int n;
    switch(type){
        case U16('S','Q') : n = write(type_fd, "SQ", 2) ; break;
        case U16('P','L') : n = write(type_fd, "PL", 2) ; break;
        case U16('I','F') : n = write(type_fd, "IF", 2) ; break;
    }
    if (n != 2) {
        perror("write type file");
        close(type_fd);
        free_str(task_dir_path);
        return -1;
    }
    
    close(type_fd);
    
    // Create argv file

    trunc_str_by(task_dir_path, 5); // remove /type
    int tmp_length = task_dir_path->length;
    char tmp_i[16];
    for(int i = 0; i < (int)nb_task ; i++){
        snprintf(tmp_i, 16,  "/%d", i);
        append(task_dir_path, tmp_i);
        if(mkdir( task_dir_path->data, 0700) != 0){
            perror("Failed mkdir in creating command");
            free_str(task_dir_path);
            return -1;
        }
        create_command_rec( task_array->tasks[task_ids[i]]->command, task_dir_path);
        trunc_str_to(task_dir_path, tmp_length);
    }


    trunc_str_to(task_dir_path, old_length);
    char tmp_j[65];
    for(int j=0 ; j < (int)nb_task; j++){
        snprintf(tmp_j, 65,  "/%ld", task_ids[j]);
        append(task_dir_path, tmp_j);
        remove_task_dir(task_dir_path);
        trunc_str_to(task_dir_path, old_length);
    }
    
    return 0;

}

