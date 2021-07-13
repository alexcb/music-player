#pragma once

#include <stdbool.h>

int open_file( const char* path, int* fd );
int open_stream( const char* url, int* fd, long int* icy_interval, char** icy_name );

