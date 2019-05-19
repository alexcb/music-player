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
	LOG_INFO("path=s loading streams", path);
	assert( sl->p == NULL );

	char *s, *key, *val;
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
		entry->url = sdsnewlen( line, s - line );

		s++;
		bool keep_going = true;
		while(keep_going) {
			key = s;
			s = strstr( s, "=" );
			if( !s ) { break; }
			*s = '\0';
			s++;
			val = s;
			s = strstr( s, " " );
			if( s ) {
				*s = '\0';
				s++;
			} else {
				keep_going = false;
			}
			LOG_DEBUG("key=s val=s got val", key, val);
			if( strcmp(key, "title") == 0 ) {
				entry->name = sdsnew( val );
			} else if( strcmp(key, "volume") == 0 ) {
				entry->volume = atof( val );
				if( entry->volume == 0 ) {
					LOG_ERROR("vol=s failed to parse volume (or value is actually 0, which is not supported)", entry->volume);
					return 1;
				}
			}
		}

		SGLIB_SORTED_LIST_ADD( Stream, sl->p, entry, STREAM_COMPARATOR, next_ptr );
	}
	fclose( fp );
	return 0;
}

int get_stream(StreamList *sl, const char *name, Stream **stream )
{
	Stream *p = sl->p;
	while( p ) {
		if (strcmp(p->name, name) == 0 ) {
			*stream = p;
			return 0;
		}
		p = p->next_ptr;
	}
	return 1;
}
