#pragma once

#include <pthread.h>

typedef struct CircularBuffer
{
	pthread_mutex_t lock;
	char *p;
	int read;
	int write;
	int size;
	int len;
} CircularBuffer;


int init_circular_buffer( CircularBuffer *buffer, size_t buffer_size );

int get_buffer_write( CircularBuffer *buffer, size_t min_buffer_size, char **p, size_t *reserved_size );
int get_buffer_read( CircularBuffer *buffer, char **p, size_t *reserved_size );

void buffer_mark_written( CircularBuffer *buffer, size_t n );
void buffer_mark_read( CircularBuffer *buffer, size_t n );
