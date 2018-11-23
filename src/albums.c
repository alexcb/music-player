#include "albums.h"

#include "errors.h"
#include "cmp.h"
#include "string_utils.h"

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>

#include "id3.h"
#include "log.h"

SGLIB_DEFINE_RBTREE_FUNCTIONS(Album,  left, right, color_field, ALBUM_CMPARATOR)
SGLIB_DEFINE_RBTREE_FUNCTIONS(Track,  left, right, color_field, TRACK_PATH_COMPARATOR)
SGLIB_DEFINE_RBTREE_FUNCTIONS(Artist, left, right, color_field, ARTIST_PATH_CMPARATOR)

void safe_basename(const char *s, sds *r)
{
	sds tmp = sdsnew( s );
	s = basename( tmp );
	*r = sdsnew( s );
	sdsfree( tmp );
}

int library_init( Library *library, ID3Cache *cache, const char *library_path )
{
	library->id3_cache = cache;
	library->artists = NULL;
	library->root_track = NULL;
	library->library_path = sdsnew(library_path);
	return 0;
}

int library_add_track( Library *library, Artist *artist, Album *album, const char *relative_track_path )
{
	int res;
	int error_code = 0;

	sds s = sdsnew( "" );

	Track *track = NULL;
	Track track_to_find;
	track_to_find.path = (sds) relative_track_path;
	track = sglib_Track_find_member( library->root_track, &track_to_find );
	if( !track ) {
		LOG_INFO( "path=s adding new track", relative_track_path );
		track = malloc(sizeof(Track));
		track->artist = artist->artist;
		track->album = album->album;
		safe_basename( relative_track_path, &(track->title) );
		track->path = sdsnew( relative_track_path );

		track->color_field = '\0';
		track->left = NULL;
		track->right = NULL; // TODO do any other of these need to be initted elsewhere?

		sglib_Track_add( &(library->root_track), track );
		SGLIB_SORTED_LIST_ADD(Track, album->tracks, track, TRACK_PATH_COMPARATOR, next_ptr);
	} else {
		LOG_INFO( "path=s checking track for changes", relative_track_path );
	}

	ID3CacheItem *id3_item;
	res = id3_cache_get( library->id3_cache, library->library_path, relative_track_path, &id3_item );
	if( res ) {
		LOG_ERROR("path=s err=s failed getting id3 tags", s, res);
		error_code = 1;
		goto error;
	}

	track->artist = id3_item->artist;
	track->title = id3_item->title;
	track->track = id3_item->track;
	track->album = id3_item->album;
	track->length = id3_item->length;

error:
	return error_code;
}

int library_add_album( Library *library, Artist *artist, const char *relative_album_path )
{
	int error_code = 0;

	struct dirent *de;
	sds s = sdsnew("");
	Album *album = NULL;
	Album album_to_find;
	album_to_find.path = (sds) relative_album_path;
	album = sglib_Album_find_member( artist->albums, &album_to_find );
	if( !album ) {
		LOG_INFO( "path=s adding new album", relative_album_path );
		album = (Album*) malloc(sizeof(Album));
		album->artist = artist->artist;
		safe_basename( relative_album_path, &(album->album) );
		album->path = sdsnew( relative_album_path );
		album->year = 0;
		album->tracks = NULL;
		album->color_field = '\0';
		album->left = NULL;
		album->right = NULL; // TODO do any other of these need to be initted elsewhere?

		sglib_Album_add( &(artist->albums), album );
	} else {
		LOG_INFO( "path=s checking album for new tracks", relative_album_path );
	}

	sdsclear( s );
	s = sdscatfmt( s, "%s/%s", library->library_path, relative_album_path );

	LOG_DEBUG( "path=s opening dir", s );
	DIR *album_dir = opendir( s );
	if( album_dir == NULL ) {
		error_code = FILESYSTEM_ERROR;
		LOG_ERROR("path=s err=s opendir failed", s, strerror(errno));
		goto error;
	}

	while( (de = readdir( album_dir )) != NULL) {
		LOG_DEBUG( "path=s t=d looping", de->d_name, de->d_type );
		if( !has_suffix( de->d_name, ".mp3" ) ) {
			continue;
		}

		// path to album
		sdsclear( s );
		s = sdscatfmt( s, "%s/%s", relative_album_path, de->d_name );
		LOG_DEBUG( "path=s calling", s );
		library_add_track( library, artist, album, s );
	}
	closedir( album_dir );

error:

	if( !album ) {
		LOG_WARN( "path=s album was never added", relative_album_path );
	} else {
		if( !album->path ) {
			LOG_WARN( "path=s album path not set", relative_album_path );
		}
		if( !album->artist ) {
			LOG_WARN( "path=s album artist not set", relative_album_path );
		}
	}

	return error_code;
}

int library_add_artist( Library *library, const char *relative_artist_path )
{
	struct dirent *album_dirent;
	int error_code = 0;
	sds s = sdsnew("");
	Artist *artist = NULL;
	Artist artist_to_find;
	artist_to_find.path = (sds) relative_artist_path;
	artist = sglib_Artist_find_member( library->artists, &artist_to_find );
	if( !artist ) {
		LOG_INFO( "path=s adding new artist", relative_artist_path );
		artist = (Artist*) malloc(sizeof(Artist));
		safe_basename( relative_artist_path, &(artist->artist) );
		artist->path = sdsnew( relative_artist_path );
		artist->albums = NULL;
		artist->color_field = '\0';
		artist->left = NULL;
		artist->right = NULL; // TODO do any other of these need to be initted elsewhere?

		sglib_Artist_add( &(library->artists), artist );
	} else {
		LOG_INFO( "path=s checking artist for new albums", relative_artist_path );
	}

	sdsclear( s );
	s = sdscatfmt( s, "%s/%s", library->library_path, relative_artist_path );
	LOG_DEBUG("path=s opening dir", s);
	DIR *album_dir = opendir( s );
	if( album_dir == NULL ) {
		error_code = FILESYSTEM_ERROR;
		LOG_ERROR("path=s err=s opendir failed", s, strerror(errno));
		goto error;
	}

	while( (album_dirent = readdir( album_dir )) != NULL) {
		if( album_dirent->d_type != DT_DIR || strcmp(album_dirent->d_name, ".") == 0 || strcmp(album_dirent->d_name, "..") == 0 ) {
			continue;
		}

		// path to album
		sdsclear( s );
		s = sdscatfmt( s, "%s/%s", relative_artist_path, album_dirent->d_name );

		library_add_album( library, artist, s );
	}
	closedir( album_dir );

error:
	sdsfree( s );
	return error_code;
}

int library_load( Library *library )
{
	int error_code = 0;
	struct dirent *artist_dirent;

	DIR *artist_dir = opendir( library->library_path );
	if( artist_dir == NULL ) {
		LOG_ERROR("path=s err=s opendir failed", library->library_path, strerror(errno));
		error_code = FILESYSTEM_ERROR;
		goto error:
	}

	while( (artist_dirent = readdir(artist_dir)) != NULL) {
		if( artist_dirent->d_type != DT_DIR || strcmp(artist_dirent->d_name, ".") == 0 || strcmp(artist_dirent->d_name, "..") == 0 ) {
			continue;
		}

		library_add_artist( library, artist_dirent->d_name );
	}
	closedir( artist_dir );
error:
	return error_code;
}

int library_get_track( Library *library, const char *path, Track **track )
{
	Track e;
	e.path = (char *) path;
	*track = sglib_Track_find_member( library->root_track, &e);
	if( *track == NULL ) {
		return 1;
	}
	return 0;
}

