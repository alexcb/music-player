#pragma once

#include <stdbool.h>

int open_fd( const char *path, int *fd, bool *is_stream, long int *icy_interval );
