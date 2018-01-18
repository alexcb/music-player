#include "circular_buffer.h"
#include "log.h"
#include "timing.h"

#include <stdlib.h>
#include <assert.h>

int init_circular_buffer( CircularBuffer *buffer, size_t buffer_size )
{
	buffer->read = 0;
	buffer->write = 0;
	buffer->size = buffer_size;
	buffer->len = 0;
	buffer->lock_reads = 0;
	buffer->read_reserved = 0;

	pthread_mutex_init( &buffer->lock, NULL );

	buffer->p = (char*) malloc( buffer_size );
	if( buffer->p == NULL ) {
		return 1;
	}
	return 0;
}

void buffer_clear( CircularBuffer *buffer )
{
	pthread_mutex_lock( &buffer->lock );

	buffer->read = 0;
	buffer->write = 0;
	buffer->len = 0;

	pthread_mutex_unlock( &buffer->lock );
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


int get_buffer_read_unsafe( CircularBuffer *buffer, char **p, size_t *reserved_size )
{
	if( buffer->read == buffer->len ) {
		buffer->read = 0;
		buffer->len = 0;
	}

	// if Writer <= Reader < Len; read from Reader to Len
	if( buffer->write <= buffer->read && buffer->read < buffer->len ) {
		*p = buffer->p + buffer->read;
		*reserved_size = buffer->len - buffer->read;
		return 0;
	}

	// Reader < Writer < Len
	if( buffer->read < buffer->write ) {
		*p = buffer->p + buffer->read;
		*reserved_size = buffer->write - buffer->read;
		return 0;
	}

	// this should only happen when len is 0 (empty buffer)
	return 1;
}

int get_buffer_read( CircularBuffer *buffer, char **p, size_t *reserved_size )
{
	int res;
	pthread_mutex_lock( &buffer->lock );
	res = get_buffer_read_unsafe( buffer, p, reserved_size );
	pthread_mutex_unlock( &buffer->lock );
	return res;
}


void buffer_mark_written( CircularBuffer *buffer, size_t n )
{
	long start = get_current_time_ms();

	pthread_mutex_lock( &buffer->lock );
	buffer->write += n;
	pthread_mutex_unlock( &buffer->lock );

	long duration = get_current_time_ms() - start;
	if( duration > 100 ) {
		LOG_WARN("buffer_mark_written took longer than 100ms");
	}
}

void buffer_mark_read( CircularBuffer *buffer, size_t n )
{
	long start = get_current_time_ms();

	pthread_mutex_lock( &buffer->lock );
	buffer->read += n;
	pthread_mutex_unlock( &buffer->lock );

	long duration = get_current_time_ms() - start;
	if( duration > 100 ) {
		LOG_WARN("buffer_mark_read took longer than 100ms");
	}
}

void buffer_mark_read_upto( CircularBuffer *buffer, char *p )
{
	long start = get_current_time_ms();

	pthread_mutex_lock( &buffer->lock );
	size_t n = p - buffer->p;
	if( n > buffer->read ) {
		buffer->read = n;
	} else if( n < buffer->read ) {
		// loop around
		buffer->read = n;
		buffer->len = 0;
	}
	//if n == buffer->read
	//  we assume nothing has happened rather than removing the whole buffer

	//LOG_DEBUG("p=p n=p buffer_mark_read_upto", p, n);
	//if( n == 0 && buffer->read != n ) {
	//	buffer->len = 0;
	//}
	//buffer->read = n;
	pthread_mutex_unlock( &buffer->lock );

	long duration = get_current_time_ms() - start;
	if( duration > 100 ) {
		LOG_WARN("buffer_mark_read_upto took longer than 100ms");
	}
}

void buffer_rewind_lock( CircularBuffer *buffer )
{
	long start = get_current_time_ms();

	pthread_mutex_lock( &buffer->lock );
	buffer->lock_reads = 1;
	pthread_mutex_unlock( &buffer->lock );

	long duration = get_current_time_ms() - start;
	if( duration > 100 ) {
		LOG_WARN("buffer_rewind_lock took longer than 100ms");
	}
}

int buffer_rewind_unsafe( CircularBuffer *buffer, char *p )
{
	// TODO add assertions to verify rewind is valid and not before reader location
	int w = p - buffer->p;
	if( w <= buffer->write ) {
		buffer->write = w;
		return 0;
	}
	buffer->write = w;
	buffer->len = 0;

	return 0;

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

	// Reader < Writer
	if( buffer->read < buffer->write ) {
		*p1 = buffer->p + buffer->read;
		*size1 = buffer->write - buffer->read;
		*p2 = NULL;
		*size2 = 0;
		return 0;
	}

	// nothing to read
	*p1 = NULL;
	*size1 = 0;
	*p2 = NULL;
	*size2 = 0;
	return 0;
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

int buffer_lock( CircularBuffer *buffer )
{
	return pthread_mutex_lock( &buffer->lock );
}

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


