#include "line_reader.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//int read_line( int fd, char *buf, int buf_size )
//{
//	int i = 0;
//	int n;
//	char c;
//	for(;;) {
//		n = read( fd, &c, 1 );
//		if( n < 0 ) {
//			return n;
//		}
//		if( n == 0 ) {
//			buf[i++] = '\0';
//			return 1;
//		}
//		if( c == '\r' ) {
//			//ignore it
//		} else if( c == '\n' ) {
//			buf[i++] = '\0';
//			return 0;
//		} else {
//			buf[i++] = c;
//		}
//	}
//}

int dump_buffer( BufferedStreamer* self )
{
	for( int i = 0; i < self->i; i++ ) {
		printf( "%02x ", ( (u_int8_t*)self->buf )[i] );
	}
	printf( "\n\n" );
}

int refresh_buffer( BufferedStreamer* self )
{
	int l = self->cap - self->i;
	int n = read( self->fd, &self->buf[self->i], l );
	if( n < 0 ) {
		if( errno == EAGAIN || errno == EWOULDBLOCK ) {
			return BUFFERED_STREAM_EAGAIN;
		}
		printf( "read failed: %d\n", n );
		return BUFFERED_STREAM_EUNKNOWN;
	}
	self->i += n;
	//printf("read worked (buf=%.*s)\n", self->i, self->buf);
	//printf("-----------\n");
	fflush( 0 );
	return BUFFERED_STREAM_OK;
}

int init_buffered_stream( BufferedStreamer* self, int fd, size_t cap )
{
	self->cap = cap;
	self->buf = malloc( cap );
	reset_buffered_stream( self, fd );
	return BUFFERED_STREAM_OK;
}
int reset_buffered_stream( BufferedStreamer* self, int fd )
{
	self->i = 0;
	self->fd = fd;
	fcntl( fd, F_SETFL, O_NONBLOCK );
}

int scan_buf_for_line( BufferedStreamer* self, char* buf, size_t* n )
{
	char* p = self->buf;
	//printf("scanning: %.*s\n", self->i, self->buf);
	for( int i = 0; i < self->i; i++ ) {
		if( p[i] == '\n' ) {
			memcpy( buf, p, i );
			buf[i] = '\0';
			*n = i;

			int l = self->i - i - 1;
			//printf("before: %.*s\n", self->i, self->buf);
			//printf("moving: %.*s\n", (int)l, &self->buf[i+1]);
			memmove( p, &p[i + 1], l );
			self->i = l;
			//printf("after: %.*s\n", self->i, self->buf);
			return 0;
		}
	}
	return 1;
}

int chomp( char* buf, int n )
{
	int i = n - 1;
	while( i >= 0 ) {
		if( buf[i] == '\r' || buf[i] == '\n' ) {
			//printf("removing char at %d\n", i);
			buf[i] = '\0';
			n = i;
			i--;
		}
		else {
			break;
		}
	}
	assert( n == strlen( buf ) );
	return n;
}

int read_line( BufferedStreamer* self, char* buf, size_t* n )
{
	int res;
	for( ;; ) {
		if( scan_buf_for_line( self, buf, n ) == 0 ) {
			*n = chomp( buf, *n );
			//dump_buffer( self );
			return BUFFERED_STREAM_OK;
		}
		if( ( res = refresh_buffer( self ) ) != BUFFERED_STREAM_OK ) {
			//printf("giving up\n");
			return res;
		}
	}
}

int read_bytes( BufferedStreamer* self, size_t n, char* buf )
{
	//printf("reading %ld\n", n);
	//dump_buffer( self );
	int res;
	if( n == 0 ) {
		return BUFFERED_STREAM_OK;
	}
	for( ;; ) {
		if( self->i >= n ) {
			memcpy( buf, self->buf, n );
			int l = self->i - n;
			memmove( self->buf, &self->buf[n], l );
			self->i -= n;
			return BUFFERED_STREAM_OK;
		}
		if( ( res = refresh_buffer( self ) ) != BUFFERED_STREAM_OK ) {
			return res;
		}
	}
}
