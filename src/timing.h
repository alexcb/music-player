#pragma once

#include <stdint.h>

struct timespec;

uint64_t get_current_time_ms( void );
uint64_t timespec_to_ms( const struct timespec* tspec );
void add_ms( struct timespec* ts, int ms );
