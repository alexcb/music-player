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
int open_stream( const char *url, int *fd, long int *icy_interval );

int open_fd( const char *path, int *fd, bool *is_stream, long int *icy_interval )
{
	int res;
	if( strstr(path, "http://") ) {
		*is_stream = true;
		res = open_stream( path, fd, icy_interval );
	} else {
		*is_stream = false;
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
int open_stream( const char *url, int *fd, long int *icy_interval )
{
	LOG_DEBUG( "url=s open_stream", url );
	*fd = http_open(url, &hd);
	*icy_interval = hd.icy_interval;
	//printf("setting icy %ld\n", hd.icy_interval);
	//if(MPG123_OK != mpg123_param(mh, MPG123_ICY_INTERVAL, hd.icy_interval, 0)) {
	//	printf("unable to set icy interval\n");
	//	return 1;
	//}
	//if(mpg123_open_fd(mh, *fd) != MPG123_OK) {
	//	printf("error\n");
	//	return 1;
	//}

	return 0;
}
