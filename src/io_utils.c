#include "io_utils.h"
#include "log.h"
#include "httpget.h"

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <assert.h>

int open_file( const char *path, int *fd );
int open_stream( const char *url, int *fd, long int *icy_interval, char **icy_name );

int open_fd( const char *path, int *fd, long int *icy_interval, char **icy_name )
{
	int res;
	if( strstr(path, "http://") ) {
		res = open_stream( path, fd, icy_interval, icy_name );
	} else {
		res = open_file( path, fd );
		*icy_interval = 0;
	}
	return res;
}

int open_file( const char *path, int *fd )
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
int open_stream( const char *url, int *fd, long int *icy_interval, char **icy_name )
{
	LOG_DEBUG( "url=s open_stream", url );
	*fd = http_open(url, &hd);
	*icy_interval = hd.icy_interval;
	*icy_name = hd.icy_name.p;

	return 0;
}
