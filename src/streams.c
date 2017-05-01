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

int parse_streams(const char *path, StreamList *sl)
{
	FILE *fp = fopen( path, "r" );
	if( !fp ) {
		LOG_ERROR( "path=s failed to open streams list for reading", path );
		return 1;
	}
	assert( sl->p == NULL );

	char *s;
	char *line = NULL;
	Stream *entry;
	size_t len = 0;
	while( getline( &line, &len, fp ) != -1 ) {
		trim_suffix( line, "\n" );
		LOG_DEBUG("line=s got line", line);
		if( !line[0] )
			continue;
		entry = malloc(sizeof(Stream));
		s = strstr( line, " " );
		entry->name = sdsnewlen( line, s - line );
		entry->url = sdsnew( s + 1 );

		SGLIB_SORTED_LIST_ADD( Stream, *p, entry, STREAM_COMPARATOR, next_ptr );
	}
	fclose( fp );
	return 0;
}
