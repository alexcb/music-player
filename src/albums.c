#include "albums.h"

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define INITIAL_ALBUM_LIST_CAP 1024
//#define INITIAL_ALBUM_LIST_CAP 5000

int album_list_init( AlbumList *album_list )
{
	album_list->list = (Album*) malloc(sizeof(Album) * INITIAL_ALBUM_LIST_CAP);
	if( album_list->list == NULL ) {
		return 1;
	}
	album_list->len = 0;
	album_list->cap = INITIAL_ALBUM_LIST_CAP;
	return 0;
}

int album_list_grow( AlbumList *album_list )
{
	if( album_list->len == album_list->cap ) {
		size_t new_cap = album_list->cap * 2;
		Album *new_list = (Album*) realloc(album_list->list, sizeof(Album) * new_cap);
		if( new_list == NULL ) {
			return 1;
		}
		album_list->list = new_list;
		album_list->cap = new_cap;
	}
	return 0;
}

int album_list_add( AlbumList *album_list, const char *artist, const char *name, const char *path )
{
	int res = album_list_grow( album_list );
	if( res ) {
		return res;
	}

	Album *album = &(album_list->list[album_list->len]);

	album->artist = strdup(artist);
	if( album->artist == NULL ) {
		return OUT_OF_MEMORY;
	}

	album->name = strdup(name);
	if( album->name == NULL ) {
		free( album->artist );
		return OUT_OF_MEMORY;
	}

	album->path = strdup(path);
	if( album->path == NULL ) {
		free( album->name );
		free( album->artist );
		return OUT_OF_MEMORY;
	}

	album_list->len++;
	return OK;
}

int album_cmp( const void *a, const void *b )
{
	const Album *aa = (const Album*) a;
	const Album *bb = (const Album*) b;

	int res = strcmp( aa->artist, bb->artist );
	if( res != 0 ) {
		return res;
	}
	return strcmp( aa->name, bb->name );
}

int album_list_sort( AlbumList *album_list )
{
	qsort( album_list->list, album_list->len, sizeof(Album), album_cmp);
	return 0;
}
