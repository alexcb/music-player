#include "albums.h"

#include "errors.h"
#include "cmp.h"

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

#include "id3.h"
#include "log.h"

SGLIB_DEFINE_RBTREE_FUNCTIONS(Album,  left, right, color_field, ALBUM_CMPARATOR)
SGLIB_DEFINE_RBTREE_FUNCTIONS(Track,  left, right, color_field, TRACK_PATH_COMPARATOR)
SGLIB_DEFINE_RBTREE_FUNCTIONS(Artist, left, right, color_field, ARTIST_PATH_CMPARATOR)


int album_list_init( AlbumList *album_list, ID3Cache *cache, const char *library_path )
{
	album_list->id3_cache = cache;
	album_list->root = NULL;
	album_list->root_track = NULL;
	album_list->library_path = sdsnew(library_path);
	return 0;
}

int setup_album( AlbumList *album_list, Album *album )
{
	int res;
	struct dirent *dent;

	album->artist = "unknown";
	album->album = "unknown";
	album->year = 0;

	sds full_path = sdscatfmt(NULL, "%s/%s", album_list->library_path, album->path);

	LOG_INFO("path=s reading album", full_path);
	DIR *d = opendir( full_path );
	if( d == NULL ) {
		LOG_ERROR("path=s err=s opendir failed", full_path, strerror(errno));
		return FILESYSTEM_ERROR;
	}

	sds s = sdsnew("");

	while( (dent = readdir(d)) != NULL) {
		if( dent->d_type == DT_DIR || strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0 ) {
			continue;
		}

		sdsclear( s );
		s = sdscatfmt( s, "%s/%s", album->path, dent->d_name );

		ID3CacheItem *id3_item;
		res = id3_cache_get( album_list->id3_cache, album_list->library_path, s, &id3_item );
		if( res ) {
			LOG_ERROR("path=s err=s failed getting id3 tags", s, d);
			goto error;
		}

		// TODO why can't the track just point to the id3_item instead of making new copies?
		Track *track = malloc(sizeof(Track));
		track->artist = id3_item->artist;
		track->title = id3_item->title;
		track->path = id3_item->path;
		track->track = id3_item->track;
		track->album = id3_item->album;
		track->length = id3_item->length;

		album->artist = id3_item->artist;
		album->album = id3_item->album;
		album->year = id3_item->year;

		SGLIB_SORTED_LIST_ADD(Track, album->tracks, track, TRACK_PATH_COMPARATOR, next_ptr);
		sglib_Track_add( &(album_list->root_track), track );
	}

	res = 0;
error:
	sdsfree( s );
	closedir( d );
	return res;
}

int album_list_load( AlbumList *album_list )
{
	Artist *artist;
	struct dirent *artist_dirent, *album_dirent;

	DIR *artist_dir = opendir(album_list->library_path);
	if( artist_dir == NULL ) {
		LOG_ERROR("path=s err=s opendir failed", album_list->library_path, strerror(errno));
		return FILESYSTEM_ERROR;
	}

	sds s = sdsnew("");

	while( (artist_dirent = readdir(artist_dir)) != NULL) {
		if( artist_dirent->d_type != DT_DIR || strcmp(artist_dirent->d_name, ".") == 0 || strcmp(artist_dirent->d_name, "..") == 0 ) {
			continue;
		}


		sdsclear( s );
		s = sdscatfmt( s, "%s/%s", album_list->library_path, artist_dirent->d_name );

		artist = (Artist*) malloc(sizeof(Artist));
		artist->artist = NULL;
		artist->path = sdsnew( s );
		artist->albums = NULL;
		artist->color_field = '\0';
		artist->left = NULL;
		artist->right = NULL; // TODO do any other of these need to be initted elsewhere?
		
		LOG_DEBUG("path=s opening dir", s);
		DIR *album_dir = opendir(s);
		if( artist_dir == NULL ) {
			LOG_ERROR("path=s err=s opendir failed", s, strerror(errno));
			sdsfree( s );
			return FILESYSTEM_ERROR;
		}

		while( (album_dirent = readdir( album_dir )) != NULL) {
			if( album_dirent->d_type != DT_DIR || strcmp(album_dirent->d_name, ".") == 0 || strcmp(album_dirent->d_name, "..") == 0 ) {
				continue;
			}

			// path to album
			sdsclear( s );

			// save album
			Album *album = (Album*) malloc(sizeof(Album));
			if( album == NULL ) {
				LOG_ERROR("allocation failed");
				closedir( album_dir );
				return 1;
			}
			memset( album, 0, sizeof(Album) );
			album->path = sdscatfmt( NULL, "%s/%s", artist_dirent->d_name, album_dirent->d_name );
			setup_album( album_list, album );
			artist->artist = album->artist;
			sglib_Album_add( &(artist->albums), album );
		}
		closedir( album_dir );

		if( artist->artist != NULL ) {
			sglib_Artist_add( &(album_list->root), artist );
		} else {
			LOG_WARN("path=s no albums found", s);
		}
	}
	closedir( artist_dir );

	return 0;
}

int album_list_get_track( AlbumList *album_list, const char *path, Track **track )
{
	Track e;
	e.path = (char *) path;
	*track = sglib_Track_find_member( album_list->root_track, &e);
	if( *track == NULL ) {
		return 1;
	}
	return 0;
}

