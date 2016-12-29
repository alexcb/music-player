#include "icy.h"
#include "errors.h"
#include <string.h>

#define STREAM_TITLE_PREFIX "StreamTitle='"
#define STREAM_TITLE_SUFFIX "';"

int parse_icy( const char *icy_meta, char **station )
{
	char *p = strstr( icy_meta, STREAM_TITLE_PREFIX );
	if( !p ) {
		return ICY_PARSE_ERROR;
	}
	p += strlen( STREAM_TITLE_PREFIX );

	char *q = strstr( icy_meta, STREAM_TITLE_SUFFIX );
	if( !q ) {
		return ICY_PARSE_ERROR;
	}
	size_t len = q - p;
	*station = strndup( p, len );
	if( *station == NULL ) {
		return OUT_OF_MEMORY;
	}

	return OK;
}
