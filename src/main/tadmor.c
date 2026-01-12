#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <endian.h>
#include <time.h>

#include "str_util.h"
#include "tube_util.h"
#include <bits/getopt_core.h>


string_t *PIPES_DIRECTORY;


/* Send a LIST request and display response */
int handle_list(int req_fd, string_t *rep_pipe_path) {  
  
    buffer_t *msg = init_buf();
    if (!msg) {
        return -1;
    }  

    if (write16(msg, OP_LIST) != 0) {
        free_buf(msg);  
        return -1;
    }


    if (write_atomic_chunks(req_fd, msg->data, msg->length) != 0) {
        free_buf(msg);
        return -1;  
    }
    free_buf(msg);  

    int rep_fd = open(rep_pipe_path->data, O_RDONLY);
    if (rep_fd < 0) {
        perror("open reply pipe");  
        return -1;  
    }
    
    uint16_t anstype;
    int r = read16(rep_fd, &anstype);
    
    if (r != 0) {
        fprintf(stderr, "Failed to read answer type\n");
        close(rep_fd);
        return -1;
    }
    
    if (anstype != ANS_OK) {
        fprintf(stderr, "Error response from daemon\n");
        close(rep_fd);
        return -1;
    }
    // Read NBTASKS
    uint32_t nbtasks;
    if (read32(rep_fd, &nbtasks) != 0) {
        fprintf(stderr, "Failed to read nbtasks\n");
        close(rep_fd);
        return -1;
    }
    // Read and display each task
    for (uint32_t i = 0; i < nbtasks; i++) {
        
        // Read TASKID
        uint64_t taskid;
        if (read64(rep_fd, &taskid) != 0){
            close(rep_fd);
            return -1;
        }

        // Read TIMING
        uint64_t minutes;
        uint32_t hours;
        uint8_t days;
        
        if (read64(rep_fd, &minutes) != 0){
            close(rep_fd);
            return -1;
        }
        
        if (read32(rep_fd, &hours) != 0){
            close(rep_fd);
            return -1;
        }
        
        unsigned char dbuf[1];
        if (read(rep_fd, dbuf, 1) != 1){
            close(rep_fd);
            return -1;
        }
        days = dbuf[0];
        
        // Read COMMANDLINE
        string_t *cmdline = init_str();
        if (read_command(rep_fd, cmdline) != 0) {
            free_str(cmdline);
            close(rep_fd);
            return -1;
        }
        
        // Format and display
        char min_str[256], hrs_str[128], day_str[32];
        
        if (minutes == 0 && hours == 0 && days == 0) {
            snprintf(min_str, sizeof(min_str), "-");
            snprintf(hrs_str, sizeof(hrs_str), "-");
            snprintf(day_str, sizeof(day_str), "-");
        } else {
            bitmap_to_string(minutes, 59, min_str, sizeof(min_str));
            bitmap_to_string(hours, 23, hrs_str, sizeof(hrs_str));
            bitmap_to_string(days, 6, day_str, sizeof(day_str));
        }
        printf("%lu: %s %s %s %s\n", taskid, min_str, hrs_str, day_str, cmdline->data);
        fflush(stdout);

        free_str(cmdline);
    }
    
    close(rep_fd);
    return 0;
}

/* Send TIMES_EXITCODES request and display response */
int handle_times_exitcodes(int req_fd, string_t *rep_pipe_path, uint64_t taskid) {
    buffer_t *msg = init_buf();
    if (!msg) return -1;
    
    if (write16(msg, OP_TIMES_EXITCODES) != 0) {
        free_buf(msg);
        return -1;
    }
    
    if (write64(msg, taskid) != 0) {
        free_buf(msg);
        return -1;
    }
    
    if (write_atomic_chunks(req_fd, msg->data, msg->length) != 0) {
        free_buf(msg);
        return -1;
    }
    free_buf(msg);
    
    // Read response

    int rep_fd = open(rep_pipe_path->data, O_RDONLY);

    uint16_t anstype;
    if (read16(rep_fd, &anstype) != 0){
        close(rep_fd);
        return -1;
    }
    if (anstype == ANS_ERROR) {
        uint16_t errcode;
        if (read16(rep_fd, &errcode) != 0) return -1;
        if (errcode == ERR_NOT_FOUND) {
            fprintf(stderr, "Task not found\n");
        } else {
            fprintf(stderr, "Unknown error\n");
        }
        close(rep_fd);
        return 1;
    }
    
    // Read NBRUNS
    uint32_t nbruns;
    if (read32(rep_fd, &nbruns) != 0){
        close(rep_fd);
        return -1;
    }

    // Read and display each run
    for (uint32_t i = 0; i < nbruns; i++) {
        // Lire 8 bytes pour timestamp
        uint64_t be_timestamp;
        unsigned char time_buf[8];
        size_t total = 0;
        while (total < 8) {
            ssize_t n = read(rep_fd, time_buf + total, 8 - total);
            if (n <= 0) {
                fprintf(stderr, "Error reading timestamp\n");
                close(rep_fd);
                return -1;
            }
            total += n;
        }
        memcpy(&be_timestamp, time_buf, 8);
        uint64_t timestamp = be64toh(be_timestamp);
        
        uint16_t be_exitcode;
        unsigned char exit_buf[2];
        total = 0;
        while (total < 2) {
            ssize_t n = read(rep_fd, exit_buf + total, 2 - total);
            if (n <= 0) {
                fprintf(stderr, "Error reading exitcode\n");
                close(rep_fd);
                return -1;
            }
            total += n;
        }
        memcpy(&be_exitcode, exit_buf, 2);
        uint16_t exitcode = be16toh(be_exitcode);
        
        // Convert timestamp to local time
        time_t t = (time_t)timestamp;
        struct tm *tm = localtime(&t);
        if (!tm) {
            fprintf(stderr, "Error converting time for timestamp %lu\n", timestamp);
            continue;
        }
        
        // Format: YYYY-MM-DD HH:MM:SS exitcode
        printf("%04d-%02d-%02d %02d:%02d:%02d %u\n",
               tm->tm_year + 1900, 
               tm->tm_mon + 1, 
               tm->tm_mday,
               tm->tm_hour, 
               tm->tm_min, 
               tm->tm_sec,
               exitcode);
    }
    close(rep_fd);
    return 0;
}
/* Send STDOUT or STDERR request and display response */
int handle_output(int req_fd, string_t *rep_pipe_path, uint64_t taskid, int is_stdout) {
    buffer_t *msg = init_buf();
    
    if (write16(msg, is_stdout ? OP_STDOUT : OP_STDERR) != 0) {
        free_buf(msg);
        return -1;
    }
    
    if (write64(msg, taskid) != 0) {
        free_buf(msg);
        return -1;
    }
    
    if (write_atomic_chunks(req_fd, msg->data, msg->length) != 0) {
        free_buf(msg);
        return -1;
    }
    free_buf(msg);
    

    // Read response

    int rep_fd = open(rep_pipe_path->data, O_RDONLY);

    uint16_t anstype;
    if (read16(rep_fd, &anstype) != 0){
        close(rep_fd);
        return -1;
    }

    if (anstype == ANS_ERROR) {
        uint16_t errcode;
        if (read16(rep_fd, &errcode) != 0) return -1;
        if (errcode == ERR_NOT_FOUND) {
            fprintf(stderr, "Task not found\n");
        } else if (errcode == ERR_NOT_RUN) {
            fprintf(stderr, "Task has not been executed yet\n");
        }

        close(rep_fd);
        return 1;
    }
    
    // Read OUTPUT
    uint32_t len;
    if (read32(rep_fd, &len) != 0) return -1;

    char *output = malloc(len + 1);

    char *p = output;
    size_t remaining = len;

    while (remaining > 0) {
        ssize_t nb = read(rep_fd, p, remaining);
        if (nb == 0) {
            // EOF reached unexpectedly
            fprintf(stderr, "Unexpected EOF\n");
            free(output);
            close(rep_fd);
            return -1;
        }
        remaining -= nb;
        p += nb;
    }

    output[len] = '\0';
    printf("%s", output);
    free(output);    
    close(rep_fd);
    return 0;
}

/* Send REMOVE request */
int handle_remove(int req_fd, string_t *rep_pipe_path, uint64_t taskid) {
    buffer_t *msg = init_buf();
    if (!msg) return -1;
    
    if (write16(msg, OP_REMOVE) != 0) {
        free_buf(msg);
        return -1;
    }
    
    if (write64(msg, taskid) != 0) {
        free_buf(msg);
        return -1;
    }
    
    if (write_atomic_chunks(req_fd, msg->data, msg->length) != 0) {
        free_buf(msg);
        return -1;
    }
    free_buf(msg);
    
    // Read response


    int rep_fd = open(rep_pipe_path->data, O_RDONLY);

    uint16_t anstype;
    if (read16(rep_fd, &anstype) != 0) return -1;
    
    if (anstype == ANS_ERROR) {
        uint16_t errcode;
        if (read16(rep_fd, &errcode) != 0){
            close(rep_fd);
            return -1;
        }

        if (errcode == ERR_NOT_FOUND) {
            fprintf(stderr, "Task not found\n");
        }
        close(rep_fd);
        return 1;
    }
    printf("Task removed\n");
    close(rep_fd);
    return 0;
}

/* Send CREATE request */
int handle_create(int req_fd, string_t *rep_pipe_path, string_t* minutes, string_t* hours, string_t* days, uint32_t argc, string_t **argv, int abstract_on) {
    buffer_t *msg = init_buf();

    if (write16(msg, OP_CREATE) != 0) {
        free_buf(msg);
        return -1;
    }
    uint64_t min_bitmap = 0;
    uint32_t hrs_bitmap = 0;
    uint8_t  day_bitmap = 0;

    if (minutes && minutes->data && !abstract_on) {
        min_bitmap = str_min_to_bitmap(minutes->data);
    }
    if (hours && hours->data && !abstract_on) {
        hrs_bitmap = str_hours_to_bitmap(hours->data);
    }
    if (days && days->data && !abstract_on) {
        day_bitmap = str_days_to_bitmap(days->data);
    }

    write64(msg, min_bitmap);
    write32(msg, hrs_bitmap);
    appendn(msg, &day_bitmap,1);

    write32(msg, (uint32_t) argc);
    for (int i = 0; i < (int)argc; i++) {
        uint32_t len = (uint32_t)strlen(argv[i]->data);
        write32(msg, len);
        appendn(msg, argv[i]->data, len);
    }
    if (write_atomic_chunks(req_fd, msg->data, msg->length) != 0) {
        free_buf(msg);
        return -1;
    }
    free_buf(msg);
    
    // Read response
    
    int rep_fd = open(rep_pipe_path->data, O_RDONLY);

    uint16_t anstype;
    if (read16(rep_fd, &anstype) != 0) return -1;
    
    if (anstype == ANS_ERROR) {
        uint16_t errcode;
        if (read16(rep_fd, &errcode) != 0){
            close(rep_fd);
            return -1;
        }
        close(rep_fd);
        return 1;
    }
    printf("Task created successfully\n");
    close(rep_fd);
    return 0;
}

/* Send COMBINE request */
int handle_combine(int req_fd, string_t *rep_pipe_path, string_t* minutes, string_t* hours, string_t* days, uint32_t nb_task, uint64_t* task_ids, int abstract_on, char type) {
    buffer_t *msg = init_buf();

    if (write16(msg, OP_COMBINE) != 0) {
        free_buf(msg);
        return -1;
    }
    uint64_t min_bitmap = 0;
    uint32_t hrs_bitmap = 0;
    uint8_t  day_bitmap = 0;

    if (minutes && minutes->data && !abstract_on) {
        min_bitmap = str_min_to_bitmap(minutes->data);
    }
    if (hours && hours->data && !abstract_on) {
        hrs_bitmap = str_hours_to_bitmap(hours->data);
    }
    if (days && days->data && !abstract_on) {
        day_bitmap = str_days_to_bitmap(days->data);
    }

    write64(msg, min_bitmap);
    write32(msg, hrs_bitmap);
    appendn(msg, &day_bitmap,1);
     
    switch(type){
        case 's' : write16(msg, U16('S','Q')); break;
        case 'p' : write16(msg, U16('P','L'));break;
        case 'i' : write16(msg, U16('I','F'));break;
        default : break;
    }

    write32(msg, nb_task);

    for( int i=0; i < (int)nb_task ; i++){
        write64(msg, task_ids[i]);
    }

    if (write_atomic_chunks(req_fd, msg->data, msg->length) != 0) {
        free_buf(msg);
        return -1;
    }
    free_buf(msg);

    //Response
    int rep_fd = open(rep_pipe_path->data, O_RDONLY);

    uint16_t anstype;
    if (read16(rep_fd, &anstype) != 0) return -1;
    
    if (anstype == ANS_ERROR) {
        uint16_t errcode;
        if (read16(rep_fd, &errcode) != 0){
            close(rep_fd);
            return -1;
        }
        close(rep_fd);
        return 1;
    }

    printf("Combined task created successfully\n");
    close(rep_fd);

    return -1;
}

/* Send TERMINATE request */
int handle_terminate(int req_fd, string_t *rep_pipe_path) {
    buffer_t *msg = init_buf();
    if (!msg) return -1;
    
    if (write16(msg, OP_TERMINATE) != 0) {
        free_buf(msg);
        return -1;
    }
    
    if (write_atomic_chunks(req_fd, msg->data, msg->length) != 0) {
        free_buf(msg);
        return -1;
    }
    free_buf(msg);

    // Read response
    int rep_fd = open(rep_pipe_path->data, O_RDONLY);

    uint16_t anstype;
    
    if (read16(rep_fd, &anstype) != 0) {
        close(rep_fd);
        return -1;
    }
    if (anstype != ANS_OK) {
        fprintf(stderr, "Error response from daemon\n");
        close(rep_fd);
        return -1;
    }   
    printf("Terminated successfully\n");
    printf("ᯓ  ✈︎\n");
    close(rep_fd);
    return 0;
}

int handle_message(int req_fd, string_t *rep_pipe_path, uint16_t msg_type){
    buffer_t *msg = init_buf();
    if (!msg) return -1;
    
    
    if (write16(msg, OP_MESSAGE) != 0) {
        free_buf(msg);
        return -1;
    }

    if (write16(msg, msg_type) != 0) {
        free_buf(msg);
        return -1;
    }

    if (write_atomic_chunks(req_fd, msg->data, msg->length) != 0) {
        free_buf(msg);
        return -1;
    }
    free_buf(msg);

    // Read response
    int rep_fd = open(rep_pipe_path->data, O_RDONLY);

    uint16_t anstype;
    
    if (read16(rep_fd, &anstype) != 0) {
        close(rep_fd);
        return -1;
    }
    if (anstype != ANS_OK) {
        fprintf(stderr, "Error response from daemon\n");
        close(rep_fd);
        return -1;
    }   
    
    close(rep_fd);
    return 0;


}

int main(int argc, char **argv) {

    PIPES_DIRECTORY = new_str("/tmp/");
    append(PIPES_DIRECTORY, getenv("USER"));
    append(PIPES_DIRECTORY, "/erraid/pipes");
    /* Check for -p option, before so it doesn't affect getopt  */
    for( int i = 0; i<argc-1 ; i++){
        if (!strcmp(argv[i], "-P")) {
            set_str(PIPES_DIRECTORY, argv[i+1]);
        }
    }

    string_t *req_pipe_path = new_str(PIPES_DIRECTORY->data);
    append(req_pipe_path, "/erraid-request-pipe");
    string_t *rep_pipe_path = new_str(PIPES_DIRECTORY->data);
    append(rep_pipe_path, "/erraid-reply-pipe");


    int req_fd = open(req_pipe_path->data, O_WRONLY);

    if (req_fd < 0) {
        perror("open req_pipe");
        return 1;
    }
    
    int opt;
    int ret = 0;
    opterr = 0; 
    while ((opt = getopt(argc, argv, ":lWwqr:x:o:e:c:s:p:i:P:")) != -1) {
        switch (opt) {
        case 'l':{
            ret = handle_list(req_fd, rep_pipe_path);
            break;
        }
        case 'q': {
            ret = handle_terminate(req_fd, rep_pipe_path);
            break;
        }
        case 'W':
        case 'w': {
            ret = handle_message(req_fd, rep_pipe_path, 1);
            break;
        }
        case 'r': {
            if (!optarg)  goto error;
            uint64_t taskid = strtoull(optarg, NULL, 10);
            ret = handle_remove(req_fd, rep_pipe_path, taskid);
            break;
        }

        case 'x': {
            if (!optarg)  goto error;
            uint64_t taskid = strtoull(optarg, NULL, 10);
            ret = handle_times_exitcodes(req_fd, rep_pipe_path, taskid);
            break;
        }

        case 'o': {
            if (!optarg)  goto error;
            uint64_t taskid = strtoull(optarg, NULL, 10);
            ret = handle_output(req_fd, rep_pipe_path, taskid, 1);
            break;
        }

        case 'e': {
            if (!optarg)  goto error;
            uint64_t taskid = strtoull(optarg, NULL, 10);
            ret = handle_output(req_fd, rep_pipe_path, taskid, 0);
            break;
        }

        case 'c': {
            if (!optarg)  goto error;
            // Si -c est défini comme "c:", optarg contient le timing
            string_t *minutes = new_str("*");
            string_t *hours = new_str("*");
            string_t *days = new_str("*");
            int abstract_on = 0;
            
            if(!strcmp(optarg, "-n")){
                abstract_on = 1;
            }else{
                optind -= 1;
            }

            /* It's at most (argc - optind) */
            string_t **cmd_argv = (string_t**)malloc(sizeof(string_t*) * (argc - optind));
            
            int continue_parsing = 1;
            uint32_t cmd_argc = 0;
            while(continue_parsing){
                while (optind < argc && argv[optind][0] != '-') {
                    cmd_argv[cmd_argc] = new_str(argv[optind]);
                    cmd_argc++;
                    optind++;

                }
                if(optind >= argc) {
                    break;
                }
                if (!strcmp(argv[optind], "-m")) {
                    set_str(minutes, argv[optind + 1]);
                    optind+=2;
                }else if (!strcmp(argv[optind], "-h") || !strcmp(argv[optind], "-H")) {
                    set_str(hours, argv[optind + 1]);
                    optind+=2;
                }else if (!strcmp(argv[optind], "-d")) {
                    set_str(days, argv[optind + 1]);
                    optind+=2;
                }else{
                    continue_parsing = 0;
                }
            }
            if (cmd_argc == 0) {
                fprintf(stderr, "Erreur : -c requiert une commande.\n");
                return 1;
            }
            ret = handle_create(req_fd, rep_pipe_path, minutes, hours, days, cmd_argc, cmd_argv, abstract_on);
            break;
        }


        case 'i':
        case 'p':
        case 's': {
            if (!optarg)  goto error;
            string_t *minutes = new_str("*");
            string_t *hours = new_str("*");
            string_t *days = new_str("*");
            int abstract_on = 0;
            if(!strcmp(optarg, "-n")){
                abstract_on = 1;
            }else{
                optind -= 1;
            }
            char type = argv[optind-1][1];

            /* It's at most argc - optind*/
            uint64_t *task_ids = malloc(sizeof(uint64_t) * (argc - optind));
            
            int continue_parsing = 1;
            uint32_t nb_task = 0;
            while(continue_parsing){
                while (optind < argc && argv[optind][0] != '-') {
                    task_ids[nb_task] = atoi(argv[optind]);
                    nb_task++;
                    optind++;

                }
                if(optind >= argc) {
                    break;
                }
                if (!strcmp(argv[optind], "-m")) {
                    set_str(minutes, argv[optind + 1]);
                    optind+=2;
                }else if (!strcmp(argv[optind], "-h") || !strcmp(argv[optind], "-H")) {
                    set_str(hours, argv[optind + 1]);
                    optind+=2;
                }else if (!strcmp(argv[optind], "-d")) {
                    set_str(days, argv[optind + 1]);
                    optind+=2;
                }else{
                    continue_parsing = 0;
                }
            }
            if (nb_task == 0) {
                fprintf(stderr, "Error : -c requires one task id.\n");
                return 1;
            }if( type == 'i' && (nb_task != 3 && nb_task != 2)){
                fprintf(stderr, "Error : -i should take exactly 2 or 3 task ids.\n");
                return 1;
            }
            
            ret = handle_combine(req_fd, rep_pipe_path, minutes, hours, days, nb_task, task_ids, abstract_on, type);
            break;
            
        }
        case 'P': {
            // Already handled
            break;
        }

        default:
            goto error;
        }
    }
    close(req_fd);  
    free_str(req_pipe_path);
    free_str(rep_pipe_path);
    free_str(PIPES_DIRECTORY);
    return ret;

error :
    close(req_fd);  
    free_str(req_pipe_path);
    free_str(rep_pipe_path);
    free_str(PIPES_DIRECTORY);
    fprintf(stderr, "Error: Unknown option '-%c'\n", optopt);
    fprintf(stderr,
        "Usage: tadmor [-p PATH] [-l|-q|-r TASKID|-x TASKID|-o TASKID|-e TASKID]\n");
    return 1;
}
