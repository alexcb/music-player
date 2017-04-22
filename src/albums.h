#pragma once

#include <stddef.h>
#include <stdbool.h>

#include "sglib.h"
#include "sds.h"

typedef struct ID3Cache ID3Cache;

typedef struct Album
{
	sds artist;
	sds album;
	sds title;
	sds path;

	bool seen;

	char color_field;
	struct ID3CacheItem *left;
	struct ID3CacheItem *right;
} Album;

#define ALBUM_CMPARATOR(x,y) strcmp((x)->path, (y)->path)
SGLIB_DEFINE_RBTREE_PROTOTYPES(Album, left, right, color_field, ALBUM_CMPARATOR)

typedef struct AlbumList
{
	Album *root;
	ID3Cache *id3_cache;
} AlbumList;

int album_list_init( AlbumList *album_list, ID3Cache *cache );
//int album_list_load( AlbumList *album_list, const char *rootpath, int *limit );
//int album_list_add( AlbumList *album_list, const char *artist, const char *name, const char *path );
//int album_list_sort( AlbumList *album_list );
