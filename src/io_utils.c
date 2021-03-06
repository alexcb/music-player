#include "io_utils.h"
#include "httpget.h"
#include "log.h"

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

int open_file( const char* path, int* fd )
{
	LOG_DEBUG( "path=s open_file", path );
	*fd = open( path, O_RDONLY );
	if( *fd < 0 ) {
		return 1;
	}
	return 0;
}

// TODO does this need to be global? can it be in the player instead?
struct httpdata hd;
int hd_needs_init = 1;
int open_stream( const char* url, int* fd, long int* icy_interval, char** icy_name )
{
	if( hd_needs_init ) {
		httpdata_init( &hd );
	} else {
		httpdata_reset( &hd );
	}
	LOG_DEBUG( "url=s open_stream", url );
	*fd = http_open( url, &hd );
	*icy_interval = hd.icy_interval;
	*icy_name = hd.icy_name.p;

	LOG_INFO( "url=s buffering stream", url );
	sleep(5);

	return 0;
}
