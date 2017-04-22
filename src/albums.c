#include "albums.h"

#include "errors.h"
#include "cmp.h"

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define INITIAL_ALBUM_LIST_CAP 1024
//#define INITIAL_ALBUM_LIST_CAP 5000

int album_list_init( AlbumList *album_list, ID3Cache *cache )
{
	album_list->id3_cache = cache;
	album_list->root = NULL;
	return 0;
}

//int album_list_load( AlbumList *album_list, const char *rootpath, int *limit )
//{
//	int res;
//	char artist_path[1024];
//	struct dirent *artist_dirent, *album_dirent;
//	char artist[1024];
//	char album[1024];
//
//	DIR *artist_dir = opendir(path);
//	if( artist_dir == NULL ) {
//		LOG_ERROR("path=s err=s opendir failed", path, strerror(errno));
//		return FILESYSTEM_ERROR;
//	}
//
//	int max_load = 3;
//
//	while( (artist_dirent = readdir(artist_dir)) != NULL) {
//		if( artist_dirent->d_type != DT_DIR || strcmp(artist_dirent->d_name, ".") == 0 || strcmp(artist_dirent->d_name, "..") == 0 ) {
//			continue;
//		}
//		
//		sprintf(artist_path, "%s/%s", path, artist_dirent->d_name);
//		LOG_DEBUG("path=s opening dir", artist_path);
//		DIR *album_dir = opendir(artist_path);
//		if( artist_dir == NULL ) {
//			LOG_ERROR("path=s err=s opendir failed", artist_path, strerror(errno));
//			return FILESYSTEM_ERROR;
//		}
//
//		while( (album_dirent = readdir( album_dir )) != NULL) {
//			if( album_dirent->d_type != DT_DIR || strcmp(album_dirent->d_name, ".") == 0 || strcmp(album_dirent->d_name, "..") == 0 ) {
//				continue;
//			}
//
//			// this is actually now the path to the album
//			sprintf(artist_path, "%s/%s/%s", path, artist_dirent->d_name, album_dirent->d_name);
//
//			strcpy( artist, artist_dirent->d_name );
//			strcpy( album, album_dirent->d_name );
//			get_album_id3_data_from_album_path( mh, artist_path, artist, album );
//
//			res = album_list_add( album_list, artist, album, artist_path );
//			if( res ) {
//				return res;
//			}
//			
//		}
//		closedir( album_dir );
//
//		if( max_load-- <= 0 ) break;
//	}
//	closedir( artist_dir );
//
//	return album_list_sort( album_list );
//}
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
