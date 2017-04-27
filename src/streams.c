#include "streams.h"

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <assert.h>

#include "string_utils.h"
#include "log.h"

int parse_streams(const char *path, Stream **p)
{
	FILE *fp = fopen( path, "r" );
	if( !fp ) {
		LOG_ERROR( "path=s failed to open streams list for reading", path );
		return 1;
	}
	*p = NULL;

	char *s;
	char *line = NULL;
	Stream *entry;
	size_t len = 0;
	while( getline( &line, &len, fp ) != -1 ) {
		trim_suffix( line, "\n" );
		LOG_DEBUG("line=s got line", line);
		if( !line[0] )
			continue;
		LOG_DEBUG("h1");
		entry = malloc(sizeof(Stream));
		LOG_DEBUG("h2");
		s = strstr( line, " " );
		LOG_DEBUG("h3");
		entry->name = sdsnewlen( line, s - line );
		LOG_DEBUG("h4");
		entry->url = sdsnew( s + 1 );
		LOG_DEBUG("h5");

		SGLIB_SORTED_LIST_ADD( Stream, *p, entry, STREAM_COMPARATOR, next_ptr );
		LOG_DEBUG("h6");
	}
	LOG_DEBUG("h7");
	fclose( fp );
	return 0;
}
