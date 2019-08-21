#pragma once

#include <stdbool.h>

int open_fd( const char* path, int* fd, long int* icy_interval, char** icy_name );
int open_stream( const char* url, int* fd, long int* icy_interval, char** icy_name );
