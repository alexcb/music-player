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

int album_list_init( AlbumList *album_list, ID3Cache *cache )
{
	album_list->id3_cache = cache;
	album_list->root = NULL;
	return 0;
}

int setup_album( AlbumList *album_list, Album *album )
{
	int res;
	struct dirent *dent;

	DIR *d = opendir( album->path );
	if( d == NULL ) {
		LOG_ERROR("path=s err=s opendir failed", album->path, strerror(errno));
		return FILESYSTEM_ERROR;
	}

	sds s = sdsnew("");

	album->artist = "unknown";
	album->album = "unknown";

	while( (dent = readdir(d)) != NULL) {
		if( dent->d_type != DT_DIR || strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0 ) {
			continue;
		}

		sdsclear( s );
		s = sdscatfmt( s, "%s/%s", album->path, dent->d_name );

		ID3CacheItem *ids_item;
		res = id3_cache_get( album_list->id3_cache, s, &ids_item );
		if( res ) {
			sdsfree( s );
			closedir( d );
			return res;
		}

		Track *track = malloc(sizeof(Track));
		track->artist = ids_item->artist;
		track->title = ids_item->title;
		track->path = ids_item->path;
		track->track = ids_item->track;

		album->artist = ids_item->artist;
		album->album = ids_item->album;

		SGLIB_SORTED_LIST_ADD(Track, album->tracks, track, TRACK_COMPARATOR, next_ptr);
	}

	closedir( d );
	return 0;
}

int album_list_load( AlbumList *album_list, const char *path, int *limit )
{
	if( limit && *limit == 0 ) { return 0; }
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

		if( limit ) {
			if( *limit == 0 ) break;
			(*limit)--;
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
