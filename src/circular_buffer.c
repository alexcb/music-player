#include "circular_buffer.h"
#include "log.h"
#include "timing.h"
#include "my_malloc.h"

#include <stdlib.h>
#include <assert.h>

// TODO make it so the buffer can never be 100% full
// need to always leave a one byte gap between the read and write positions
// this way when we do a mark up to its clear that it's no changing anything

void log_buffer( CircularBuffer *buffer, const char *s )
{
	LOG_DEBUG("caller=s read=d write=d size=d len=d lock_reads=d read_reserved=d data",
		s,
		buffer->read,
		buffer->write,
		buffer->size,
		buffer->len,
		buffer->lock_reads,
		buffer->read_reserved
		);
}

int init_circular_buffer( CircularBuffer *buffer, size_t buffer_size )
{
	buffer->read = 0;
	buffer->write = 0;
	buffer->size = buffer_size;
	buffer->len = 0;
	buffer->lock_reads = 0;
	buffer->read_reserved = 0;

	buffer->wake_up_get_buffer_read = false;
	buffer->wake_up_get_buffer_write = false;

	pthread_mutex_init( &buffer->lock, NULL );

	pthread_cond_init( &buffer->space_free, NULL );
	pthread_cond_init( &buffer->data_avail, NULL );

	buffer->p = (char*) my_malloc( buffer_size );
	if( buffer->p == NULL ) {
		return 1;
	}
	return 0;
}

// wake_up_get_buffer_write causes the get_buffer_write call to wake up
void wake_up_get_buffer_write( CircularBuffer *buffer )
{
	//LOG_DEBUG("setting wake_up_get_buffer_write true");
	pthread_mutex_lock( &buffer->lock );
	buffer->wake_up_get_buffer_write = true;
	pthread_mutex_unlock( &buffer->lock );
	pthread_cond_signal( &buffer->space_free );
	//LOG_DEBUG("done setting wake_up_get_buffer_write true");
}

// wake_up_get_buffer_read causes the get_buffer_read call to wake up
void wake_up_get_buffer_read( CircularBuffer *buffer )
{
	//LOG_DEBUG("setting wake_up_get_buffer_read true");
	pthread_mutex_lock( &buffer->lock );
	buffer->wake_up_get_buffer_read = true;
	pthread_mutex_unlock( &buffer->lock );
	pthread_cond_signal( &buffer->data_avail );
	//LOG_DEBUG("done setting wake_up_get_buffer_read true");
}

void buffer_clear( CircularBuffer *buffer )
{
	//long start = get_current_time_ms();

	pthread_mutex_lock( &buffer->lock );

	buffer->read = 0;
	buffer->write = 0;
	buffer->len = 0;

	pthread_mutex_unlock( &buffer->lock );
	pthread_cond_signal( &buffer->space_free );

	//long duration = get_current_time_ms() - start;
	//if( duration > 5 ) { LOG_WARN("duration=d slow call", duration); }
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
		*reserved_size = buffer->size - 1;
		assert( *p >= buffer->p && *p < (buffer->p + buffer->size) );
		return 0;
	}

	if( buffer->write > buffer->read ) {
		n = buffer->size - buffer->write - 1;
		if( n >= min_buffer_size ) {
			*reserved_size = n;
			*p = buffer->p + buffer->write;
			assert( *p >= buffer->p && *p < (buffer->p + buffer->size) );
			return 0;
		}

		if( buffer->read < 2 ) {
			return 1;
		}

		buffer->len = buffer->write;
		buffer->write = 0;
	}

	n = buffer->read - buffer->write - 1;
	if( n >= min_buffer_size ) {
		*reserved_size = n;
		*p = buffer->p + buffer->write;
		assert( *p >= buffer->p && *p < (buffer->p + buffer->size) );
		return 0;
	}

	return 1;
}

int get_buffer_write( CircularBuffer *buffer, size_t min_buffer_size, char **p, size_t *reserved_size )
{
	int res;
	res = pthread_mutex_lock( &buffer->lock );
	assert( !res );
	for(;;) {
		res = get_buffer_write_unsafe( buffer, min_buffer_size, p, reserved_size );
		if( res == 0 ) {
			break;
		}
		if( buffer->wake_up_get_buffer_write ) {
			buffer->wake_up_get_buffer_write = false;
			break; //return error code
		}
		//LOG_DEBUG("write buffer full; sleeping");
		pthread_cond_wait( &buffer->space_free, &buffer->lock );
		//LOG_DEBUG("write buffer wake up");
	}
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
	res = pthread_mutex_lock( &buffer->lock );
	assert( !res );
	for(;;) {
		res = get_buffer_read_unsafe( buffer, p, reserved_size );
		if( res == 0 ) {
			break;
		}
		if( buffer->wake_up_get_buffer_read ) {
			buffer->wake_up_get_buffer_read = false;
			break;
		}
		//LOG_DEBUG("read buffer empty; sleeping");
		pthread_cond_wait( &buffer->data_avail, &buffer->lock );
		//LOG_DEBUG("read buffer empty; wakeup");
	}
	
	pthread_mutex_unlock( &buffer->lock );

	return res;
}


void buffer_mark_written( CircularBuffer *buffer, size_t n )
{
	//long start = get_current_time_ms();

	pthread_mutex_lock( &buffer->lock );
	buffer->write += n;
	pthread_mutex_unlock( &buffer->lock );
	pthread_cond_signal( &buffer->data_avail );

	//long duration = get_current_time_ms() - start;
	//if( duration > 5 ) { LOG_WARN("duration=d slow call", duration); }
}

void buffer_mark_read( CircularBuffer *buffer, size_t n )
{
	//long start = get_current_time_ms();

	pthread_mutex_lock( &buffer->lock );
	buffer->read += n;
	pthread_mutex_unlock( &buffer->lock );
	pthread_cond_signal( &buffer->space_free );

	//long duration = get_current_time_ms() - start;
	//if( duration > 5 ) { LOG_WARN("duration=d slow call", duration); }
}

void buffer_mark_read_upto( CircularBuffer *buffer, char *p )
{
	//long start = get_current_time_ms();

	pthread_mutex_lock( &buffer->lock );
	size_t n = p - buffer->p;
	//LOG_DEBUG("p=p bufp=p n=d marking up to", p, buffer->p, n);
	if( n >= buffer->read ) {
		if( buffer->len ) {
			assert( n < buffer->len );
		} else {
			assert( n <= buffer->write );
		}
		//LOG_DEBUG("buffer_mark_read_upto no loop");
		buffer->read = n;
	} else if( n < buffer->read ) {
		assert( buffer->len );
		assert( n <= buffer->write );
		// loop around
		buffer->read = n;
		buffer->len = 0;
		//LOG_DEBUG("buffer_mark_read_upto loop"); // this has happened once when we got corrupt data
	}
	//if n == buffer->read
	//  we assume nothing has happened rather than removing the whole buffer

	//LOG_DEBUG("p=p n=p buffer_mark_read_upto", p, n);
	//if( n == 0 && buffer->read != n ) {
	//	buffer->len = 0;
	//}
	//buffer->read = n;
	pthread_mutex_unlock( &buffer->lock );
	pthread_cond_signal( &buffer->space_free );

	//long duration = get_current_time_ms() - start;
	//if( duration > 5 ) { LOG_WARN("duration=d slow call", duration); }
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


