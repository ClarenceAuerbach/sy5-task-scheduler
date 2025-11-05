[1mdiff --git a/src/main/task.c b/src/main/task.c[m
[1mindex f8ca551..8398aaf 100644[m
[1m--- a/src/main/task.c[m
[1m+++ b/src/main/task.c[m
[36m@@ -1,5 +1,3 @@[m
[31m-// #define _DEFAULT_SOURCE[m
[31m-[m
 #include <stdint.h>[m
 #include <string.h>[m
 #include <stdio.h>[m
[36m@@ -82,10 +80,11 @@[m [mint extract_cmd(command_t * dest_cmd, char * cmd_path) {[m
                 return -1;[m
             }[m
             int read_val = read(fd, &(dest_cmd->args), sizeof(arguments_t));[m
[31m-            if (read_val < sizeof(arguments_t)) {[m
[32m+[m[32m            if (read_val < (int) sizeof(arguments_t)) {[m
                 closedir(dir);[m
                 return -1;[m
             }[m
[32m+[m[32m            close(fd);[m
         }[m
 [m
         if (!strcmp(name,"type")) {[m
[36m@@ -95,15 +94,16 @@[m [mint extract_cmd(command_t * dest_cmd, char * cmd_path) {[m
                 return -1;[m
             }[m
             int read_val = read(fd, &(dest_cmd->type), sizeof(uint16_t));[m
[31m-            if (read_val < sizeof(uint16_t)) {[m
[32m+[m[32m            if (read_val < (int) sizeof(uint16_t)) {[m
                 closedir(dir);[m
                 return -1;[m
             }[m
[32m+[m[32m            close(fd);[m
         }[m
         struct stat st ;[m
[31m-        lstat(cmd_path, &st);[m
[31m-        if (st.st_mode == S_IFDIR) {[m
[31m-            char * dir_path_tmp ; [m
[32m+[m[32m        stat(cmd_path, &st);[m
[32m+[m[32m        if ( S_ISDIR(st.st_mode) ) {[m
[32m+[m[32m            char dir_path_tmp[strlen(cmd_path)] ;[m[41m [m
             strcpy(dir_path_tmp, cmd_path);[m
             [m
             extract_cmd((dest_cmd->cmd) + count , cmd_path);[m
[36m@@ -130,7 +130,7 @@[m [mint extract_task(task_t *dest_task, char *dir_path, int id){[m
         return -1;[m
     }[m
     struct dirent *entry;[m
[31m-[m
[32m+[m[41m    [m
     while ((entry = readdir(dir))) {[m
         char *name = entry->d_name;[m
         snprintf(dir_path, strlen(dir_path)+strlen(name) +1, "%s%s%s", dir_path, "/", name);[m
[36m@@ -146,16 +146,17 @@[m [mint extract_task(task_t *dest_task, char *dir_path, int id){[m
                 return -1;[m
             }[m
             int read_val = read(fd, &(dest_task->timings), sizeof(timing_t));[m
[31m-            if (read_val < sizeof(timing_t)) {[m
[32m+[m[32m            if (read_val < (int) sizeof(timing_t)) {[m
                 closedir(dir);[m
                 return -1;[m
             }[m
[32m+[m[32m            close(fd);[m
         }[m
 [m
         struct stat st ;[m
[31m-        lstat(dir_path, &st);[m
[31m-        if (st.st_mode == S_IFDIR) {[m
[31m-            char * dir_path_tmp ; [m
[32m+[m[32m        stat(dir_path, &st);[m
[32m+[m[32m        if ( S_ISDIR(st.st_mode) ) {[m
[32m+[m[32m            char * dir_path_tmp[strlen(dir_path)] ;[m[41m [m
             strcpy(dir_path_tmp, dir_path);[m
             [m
             extract_cmd(dest_task->command, dir_path);[m
