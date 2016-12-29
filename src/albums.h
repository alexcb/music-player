#pragma once

#include <stddef.h>

typedef struct Album
{
	char *artist;
	char *name;
	char *path;
} Album;

typedef struct AlbumList
{
	Album *list;
	size_t len, cap;
} AlbumList;

int album_list_init( AlbumList *album_list );
int album_list_add( AlbumList *album_list, const char *artist, const char *name, const char *path );
int album_list_sort( AlbumList *album_list );
