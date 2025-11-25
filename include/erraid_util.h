#ifndef ERRAID_UTIL_H
#define ERRAID_UTIL_H

#include "task.h"

void print_exc(char *path) ;

void printBits(size_t const size, void const * const ptr);

void print_string( string_t string);

void print_task(task_t task);

int count_dir_size(char *dir_path , int only_count_dir);

#endif