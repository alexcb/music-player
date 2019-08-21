//#include <stdio.h>
//#include <assert.h>
//
//#include "circular_buffer.h"
//
//void test_circular_buffer()
//{
//	int res;
//	CircularBuffer buffer;
//	res = init_circular_buffer( &buffer, 10 );
//	assert( res == 0 );
//
//	char *p = NULL;
//	size_t n;
//	// data: ----------
//	res = get_buffer_read( &buffer, &p, &n );
//	assert( res == 1 );
//
//	// data: ----------
//	res = get_buffer_write( &buffer, 3, &p, &n );
//	assert( res == 0 );
//	assert( p == buffer.p );
//	assert( n == 10 );
//
//	// data: ----------
//	res = get_buffer_read( &buffer, &p, &n );
//	assert( res == 1 );
//
//	// data: xxx-------
//	buffer_mark_written( &buffer, 3 );
//
//	// data: xxx-------
//	res = get_buffer_read( &buffer, &p, &n );
//	assert( res == 0 );
//	assert( n == 3 );
//	assert( p == buffer.p );
//
//	// data: --x-------
//	buffer_mark_read( &buffer, 2 );
//
//	// data: --x-------
//	res = get_buffer_read( &buffer, &p, &n );
//	assert( res == 0 );
//	assert( n == 1 );
//	assert( p == (buffer.p+2) );
//
//	// data: --xxxxxx--
//	buffer_mark_written( &buffer, 5 );
//
//	// data: --xxxxxx--
//	res = get_buffer_read( &buffer, &p, &n );
//	assert( res == 0 );
//	assert( n == 6 );
//	assert( p == (buffer.p+2) );
//
//	// data: --xxxxxx--
//	buffer_mark_read_upto( &buffer, p );
//
//	// data: --xxxxxx--
//	res = get_buffer_read( &buffer, &p, &n );
//	assert( res == 0 );
//	assert( n == 6 );
//	assert( p == (buffer.p+2) );
//
//	// data: ------xx--
//	buffer_mark_read_upto( &buffer, p+4 );
//
//	// data: ------xx--
//	res = get_buffer_read( &buffer, &p, &n );
//	assert( res == 0 );
//	assert( n == 2 );
//	assert( p == (buffer.p+6) );
//
//	// data: ------xx--
//	res = get_buffer_write( &buffer, 5, &p, &n );
//	assert( res == 0 );
//	assert( p == buffer.p );
//	assert( n == 6 );
//
//	// data: xxxx--xx--
//	buffer_mark_written( &buffer, 4 );
//
//	// data: xxxx--xx--
//	res = get_buffer_read( &buffer, &p, &n );
//	assert( res == 0 );
//	assert( n == 2 );
//	assert( p == (buffer.p+6) );
//
//	// data: xxxx------
//	buffer_mark_read_upto( &buffer, p+2 );
//
//	// data: xxxx------
//	res = get_buffer_read( &buffer, &p, &n );
//	assert( res == 0 );
//	assert( n == 4 );
//	assert( p == buffer.p );
//
//}
//
//int main(int argc, char *argv[])
//{
//	printf("running tests\n");
//
//	//test_circular_buffer();
//
//
//	return 0;
//}
