#define _POSIX_C_SOURCE 200809L

#include "timing.h"

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "log.h"

uint64_t timespec_to_ms( const struct timespec* tspec )
{
	uint64_t t;

	t = tspec->tv_sec * 1000;
	uint64_t nsec = tspec->tv_nsec;
	while( nsec > 999999999 ) {
		t += 1000;
		nsec -= 1000000000;
	}
	t += tspec->tv_nsec / 1000000;
	return t;
}

uint64_t get_current_time_ms( void )
{
	int res;
	struct timespec now;

	if( ( res = clock_gettime( CLOCK_PROCESS_CPUTIME_ID, &now ) ) ) {
		LOG_ERROR( "res=d clock_gettime failed" );
		assert( 0 );
	}

	return timespec_to_ms( &now );
}

#define NSEC_PER_SEC 1000000000
void add_ms( struct timespec* ts, int ms )
{
	ts->tv_nsec += ms * 1000000;

	ts->tv_sec += ts->tv_nsec / NSEC_PER_SEC;
	ts->tv_nsec = ts->tv_nsec % NSEC_PER_SEC;
}
