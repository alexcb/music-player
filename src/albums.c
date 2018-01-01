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


SGLIB_DEFINE_RBTREE_FUNCTIONS(Album, left, right, color_field, ALBUM_CMPARATOR)
SGLIB_DEFINE_RBTREE_FUNCTIONS(Track, left, right, color_field, TRACK_PATH_COMPARATOR)


int album_list_init( AlbumList *album_list, ID3Cache *cache )
{
	album_list->id3_cache = cache;
	album_list->root = NULL;
	album_list->root_track = NULL;
	return 0;
}

int setup_album( AlbumList *album_list, Album *album )
{
	int res;
	struct dirent *dent;

	album->artist = "unknown";
	album->album = "unknown";
	album->year = 0;

	LOG_INFO("path=s reading album", album->path);
	DIR *d = opendir( album->path );
	if( d == NULL ) {
		LOG_ERROR("path=s err=s opendir failed", album->path, strerror(errno));
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
		res = id3_cache_get( album_list->id3_cache, s, &id3_item );
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

int album_list_load( AlbumList *album_list, const char *path )
{
	struct dirent *artist_dirent, *album_dirent;

	DIR *artist_dir = opendir(path);
	if( artist_dir == NULL ) {
		LOG_ERROR("path=s err=s opendir failed", path, strerror(errno));
		return FILESYSTEM_ERROR;
	}

	sds s = sdsnew("");

	while( (artist_dirent = readdir(artist_dir)) != NULL) {
		if( artist_dirent->d_type != DT_DIR || strcmp(artist_dirent->d_name, ".") == 0 || strcmp(artist_dirent->d_name, "..") == 0 ) {
			continue;
		}

		sdsclear( s );
		s = sdscatfmt( s, "%s/%s", path, artist_dirent->d_name );
		
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
			s = sdscatfmt( s, "%s/%s/%s", path, artist_dirent->d_name, album_dirent->d_name);

			// save album
			Album *album = (Album*) malloc(sizeof(Album));
			if( album == NULL ) {
				LOG_ERROR("allocation failed");
				closedir( album_dir );
				return 1;
			}
			memset( album, 0, sizeof(Album) );
			album->path = sdsdup( s );
			setup_album( album_list, album );
			sglib_Album_add( &(album_list->root), album );
		}
		closedir( album_dir );
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

//
//
//int album_list_grow( AlbumList *album_list )
//{
//	if( album_list->len == album_list->cap ) {
//		size_t new_cap = album_list->cap * 2;
//		Album *new_list = (Album*) realloc(album_list->list, sizeof(Album) * new_cap);
//		if( new_list == NULL ) {
//			return 1;
//		}
//		album_list->list = new_list;
//		album_list->cap = new_cap;
//	}
//	return 0;
//}
//
//int album_list_add( AlbumList *album_list, const char *artist, const char *name, const char *path )
//{
//	int res = album_list_grow( album_list );
//	if( res ) {
//		return res;
//	}
//
//	Album *album = &(album_list->list[album_list->len]);
//
//	album->artist = strdup(artist);
//	if( album->artist == NULL ) {
//		return OUT_OF_MEMORY;
//	}
//
//	album->name = strdup(name);
//	if( album->name == NULL ) {
//		free( album->artist );
//		return OUT_OF_MEMORY;
//	}
//
//	album->path = strdup(path);
//	if( album->path == NULL ) {
//		free( album->name );
//		free( album->artist );
//		return OUT_OF_MEMORY;
//	}
//
//	album_list->len++;
//	return OK;
//}
//
//int album_cmp( const void *a, const void *b )
//{
//	const Album *aa = (const Album*) a;
//	const Album *bb = (const Album*) b;
//
//	int res = stricmp( aa->artist, bb->artist );
//	if( res != 0 ) {
//		return res;
//	}
//	return stricmp( aa->name, bb->name );
//}
//
//int album_list_sort( AlbumList *album_list )
//{
//	qsort( album_list->list, album_list->len, sizeof(Album), album_cmp);
//	return 0;
//}
