#include "log.h"

#include <mpg123.h>

#include <assert.h>
#include <mpg123.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <dirent.h>

#include "errors.h"
#include "httpget.h"
#include "library.h"
#include "log.h"
#include "my_data.h"
#include "player.h"
#include "playlist.h"
#include "playlist_manager.h"
#include "streams.h"
#include "string_utils.h"
#include "timing.h"
#include "web.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "id3.h"
#include "my_malloc.h"

int main( int argc, char** argv, char** env )
{
	my_malloc_init();

	char* log_level = (char*)sdsnew( getenv( "LOG_LEVEL" ) );
	str_to_upper( log_level );
	set_log_level_from_env_variables( (const char**)env );
	LOG_INFO( "here-info" );
	LOG_DEBUG( "here-debug" );
	LOG_TRACE( "here-trace" );

	int res;
	ID3Cache* cache;

	if( argc != 3 ) {
		printf( "%s <albums path> <id3 cache path>\n", argv[0] );
		return 1;
	}
	char* music_path = argv[1];
	char* id3_cache_path = argv[2];

	mpg123_init();
	mpg123_handle* mh = mpg123_new( NULL, NULL );
	if( mh == NULL ) {
		LOG_ERROR("failed to create new mpg123 handler");
		return 1;
	}

	trim_suffix( music_path, "/" );

	res = id3_cache_new( &cache, id3_cache_path, mh );
	if( res ) {
		LOG_ERROR( "failed to load cache" );
		return 1;
	}

	Library library;
	res = library_init( &library, cache, music_path );
	if( res ) {
		LOG_CRITICAL( "err=d failed to init album list", res );
		return 1;
	}
	res = library_load( &library );
	if( res ) {
		LOG_CRITICAL( "err=d failed to load albums", res );
		return 1;
	}

	res = id3_cache_save( cache );
	if( res ) {
		LOG_ERROR( "err=d path=s failed to save id3 cache", res, cache->path );
	}

	const char *issues_path = "/tmp/music-check-issues";
	FILE* fp = fopen( issues_path, "wb" );

	Track *track;
	struct sglib_Track_iterator  it;
	for(track=sglib_Track_it_init_inorder(&it,library.root_track); track!=NULL; track=sglib_Track_it_next(&it)) {
		if( track->album == NULL ) {
			fprintf( fp, "NULL_ALBUM %s\n", track->path );
			LOG_ERROR("path=s album is NULL", track->path);
			continue;
		}
		if( track->album->artist == NULL ) {
			fprintf( fp, "NULL_ARTIST %s\n", track->path );
			LOG_ERROR("path=s album artist is NULL", track->path);
			continue;
		}
		if( track->album->release_date == 0 ) {
			fprintf( fp, "MISSING_YEAR %s\n", track->path );
			LOG_WARN("path=s track is missing year", track->path);
		}
		if( strlen(track->album->album) == 0 ) {
			fprintf( fp, "MISSING_ALBUM %s\n", track->path );
			LOG_WARN("path=s track is missing album title", track->path);
		}
		if( strlen(track->album->artist->artist) == 0 ) {
			fprintf( fp, "MISSING_ARTIST %s\n", track->path );
			LOG_WARN("path=s track is missing artist", track->path);
		}
		if( strlen(track->title) == 0 ) {
			fprintf( fp, "MISSING_TITLE %s\n", track->path );
			LOG_WARN("path=s track is missing title", track->path);
		}
	}

	fclose( fp );
	LOG_INFO( "path=s wrote issues out", issues_path );

	return 0;
}
