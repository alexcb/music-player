#include "circular_buffer.h"

#include <stdlib.h>
#include <assert.h>

int init_circular_buffer( CircularBuffer *cb, size_t buffer_size )
{
	cb->read = 0;
	cb->write = 0;
	cb->size = buffer_size;
	cb->len = 0;
	cb->lock_reads = 0;
	cb->read_reserved = 0;

	pthread_mutex_init( &cb->lock, NULL );

	cb->p = (char*) malloc( buffer_size );
	if( cb->p == NULL ) {
		return 1;
	}
	return 0;
}

int get_buffer_write_unsafe( CircularBuffer *buffer, size_t min_buffer_size, char **p, size_t *reserved_size )
{
	size_t n;

	if( min_buffer_size > buffer->size / 2 ) {
		return 1;
	}
	
	// special case for empty buffer
	if( buffer->len == 0 && buffer->read == buffer->write) {
		buffer->read = buffer->write = 0;
		*p = buffer->p;
		*reserved_size = buffer->size;
		return 0;
	}

	if( buffer->write > buffer->read ) {
		n = buffer->size - buffer->write;
		if( n >= min_buffer_size ) {
			*reserved_size = n;
			*p = buffer->p + buffer->write;
			return 0;
		}

		buffer->len = buffer->write;
		buffer->write = 0;
	}

	n = buffer->read - buffer->write;
	if( n >= min_buffer_size ) {
		*reserved_size = n;
		*p = buffer->p + buffer->write;
		return 0;
	}

	return 1;
}

int get_buffer_write( CircularBuffer *buffer, size_t min_buffer_size, char **p, size_t *reserved_size )
{
	int res;
	res = pthread_mutex_lock( &buffer->lock );
	assert( !res );
	res = get_buffer_write_unsafe( buffer, min_buffer_size, p, reserved_size );
	pthread_mutex_unlock( &buffer->lock );
	return res;
}


int get_buffer_read_unsafe( CircularBuffer *buffer, size_t max_size, char **p, size_t *reserved_size )
{
	if( buffer->read == buffer->len ) {
		buffer->read = 0;
		buffer->len = 0;
	}

	// if Writer <= Reader < Len; read from Reader to Len
	if( buffer->write <= buffer->read && buffer->read < buffer->len ) {
		*p = buffer->p + buffer->read;
		*reserved_size = buffer->len - buffer->read;
		if( *reserved_size > max_size ) {
			*reserved_size = max_size;
			buffer->read_reserved = buffer->read + *reserved_size;
		}
		return 0;
	}

	// Reader < Writer < Len
	if( buffer->read < buffer->write ) {
		*p = buffer->p + buffer->read;
		*reserved_size = buffer->write - buffer->read;
		if( *reserved_size > max_size ) {
			*reserved_size = max_size;
			buffer->read_reserved = buffer->read + *reserved_size;
		}
		return 0;
	}

	// this should only happen when len is 0 (empty buffer)
	return 1;
}

int get_buffer_read( CircularBuffer *buffer, size_t max_size, char **p, size_t *reserved_size )
{
	int res;
	struct timespec tspec;
	tspec.tv_sec = 0;
	tspec.tv_nsec = 100;
	res = pthread_mutex_timedlock( &buffer->lock, &tspec );
	if( res ) {
		return res;
	}

	if( buffer->lock_reads == 1 ) {
		res = 1;
	} else {
		res = get_buffer_read_unsafe( buffer, max_size, p, reserved_size );
	}
	pthread_mutex_unlock( &buffer->lock );
	return res;
}


void buffer_mark_written( CircularBuffer *buffer, size_t n )
{
	pthread_mutex_lock( &buffer->lock );
	buffer->write += n;
	pthread_mutex_unlock( &buffer->lock );
}

void buffer_mark_read( CircularBuffer *buffer, size_t n )
{
	pthread_mutex_lock( &buffer->lock );
	buffer->read += n;
	pthread_mutex_unlock( &buffer->lock );
}

void buffer_rewind_lock( CircularBuffer *buffer )
{
	pthread_mutex_lock( &buffer->lock );
	buffer->lock_reads = 1;
	pthread_mutex_unlock( &buffer->lock );
}

int buffer_rewind_and_unlock( CircularBuffer *buffer, char *p )
{
	return 1;
//	int res = 0;
//	int write = p - buffer->p;
//	if( buffer->read <= write && write < buffer->read_reserved ) {
//		//printf("here %d %d %d\n", buffer->read, write, buffer->read_reserved);
//		res = 1;
//		goto error;
//	}
//
//	// if Writer <= Reader < Len; read from Reader to Len
//	if( buffer->write <= buffer->read ) {
//		if( write <= buffer->write ) {
//			buffer->write = write;
//		} else {
//			if( write <= buffer->read ) {
//				res = 2;
//				goto error;
//			}
//			buffer->write = write;
//			buffer->len = 0;
//		}
//	} else {
//		buffer->write = write;
//	}
//
//error:
//	pthread_mutex_lock( &buffer->lock );
//	buffer->lock_reads = 0;
//	pthread_mutex_unlock( &buffer->lock );
//	return res;
}

int get_buffer_read_unsafe2( CircularBuffer *buffer, size_t max_size, char **p1, size_t *size1, char **p2, size_t *size2 )
{
	if( buffer->read == buffer->len ) {
		*p1 = buffer->p;
		*size1 = buffer->write;
		*p2 = NULL;
		*size2 = 0;
		return 0;
	}

	// if Writer <= Reader < Len; read from Reader to Len
	if( buffer->write <= buffer->read && buffer->read < buffer->len ) {
		*p1 = buffer->p + buffer->read;
		*size1 = buffer->len - buffer->read;
		*p2 = buffer->p;
		*size2 = buffer->write;
		return 0;
	}

	// Reader < Writer < Len
	if( buffer->read < buffer->write ) {
		*p1 = buffer->p + buffer->read;
		*size1 = buffer->write - buffer->read;
		*p2 = NULL;
		*size2 = 0;
		return 0;
	}

	// should never happen
	assert(0);
	return 1;
}

void buffer_mark_read_unsafe( CircularBuffer *buffer, size_t n )
{
	if( buffer->len > 0 ) {
		size_t remaining = buffer->len - buffer->read;
		if( n >= remaining ) {
			buffer->len = 0;
			buffer->read = n - remaining;
			return;
		}
	}
	buffer->read += n;
}

//int get_buffer_non_reserved_reads( CircularBuffer *buffer, char **p1, size_t *size1, char **p2, size_t *size2 )
//{
//	if( buffer->read == buffer->len ) {
//		*p1 = buffer->p;
//		*size1 = buffer->write;
//		*p2 = NULL;
//		*size2 = 0;
//		return 0;
//	}
//
//	// if Writer <= Reader < Len; read from Reader to Len
//	if( buffer->write <= buffer->read && buffer->read < buffer->len ) {
//		*p1 = buffer->p + buffer->read;
//		*size1 = buffer->len - buffer->read;
//		*p2 = buffer->p;
//		*size2 = buffer->write;
//		return 0;
//	}
//
//	// Reader < Writer < Len
//	if( buffer->read < buffer->write ) {
//		*p1 = buffer->p;
//		*size1 = buffer->write;
//		*p2 = NULL;
//		*size2 = 0;
//		return 0;
//	}
//
//	// should never happen
//	return 1;
//}

int buffer_timedlock( CircularBuffer *buffer )
{
	struct timespec tspec;
	tspec.tv_sec = 0;
	tspec.tv_nsec = 100;
	return pthread_mutex_timedlock( &buffer->lock, &tspec );
}

int buffer_unlock( CircularBuffer *buffer )
{
	return pthread_mutex_unlock( &buffer->lock );
}


