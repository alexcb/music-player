#pragma once

#include <pthread.h>
#include <stdbool.h>

typedef struct CircularBuffer
{
	pthread_mutex_t lock;
	pthread_cond_t space_free;
	pthread_cond_t data_avail;

	bool wake_up_get_buffer_read;
	bool wake_up_get_buffer_write;

	char* p;
	int read;
	int write;
	int size;
	int len; // when len is set, this means there's valid data from current read up until this value
	int read_reserved;
	int lock_reads;
} CircularBuffer;

void wake_up_get_buffer_write( CircularBuffer* buffer );
void wake_up_get_buffer_read( CircularBuffer* buffer );

int init_circular_buffer( CircularBuffer* buffer, size_t buffer_size );
void buffer_clear( CircularBuffer* buffer );

int get_buffer_write( CircularBuffer* buffer,
					  size_t min_buffer_size,
					  char** p,
					  size_t* reserved_size );
int get_buffer_read( CircularBuffer* buffer, char** p, size_t* reserved_size );

void buffer_mark_written( CircularBuffer* buffer, size_t n );
void buffer_mark_read( CircularBuffer* buffer, size_t n );
void buffer_mark_read_upto( CircularBuffer* buffer, char* p );

int buffer_rewind_unsafe( CircularBuffer* buffer, char* p );

//int get_buffer_non_reserved_reads( CircularBuffer *buffer, char **p1, size_t *size1, char **p2, size_t *size2 );

int get_buffer_read_unsafe2(
	CircularBuffer* buffer, size_t max_size, char** p1, size_t* size1, char** p2, size_t* size2 );

void buffer_mark_read_unsafe( CircularBuffer* buffer, size_t n );

int buffer_timedlock( CircularBuffer* buffer );
int buffer_unlock( CircularBuffer* buffer );
int buffer_lock( CircularBuffer* buffer );
