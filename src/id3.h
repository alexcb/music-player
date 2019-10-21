#pragma once

#include <mpg123.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "sds.h"
#include "sglib.h"

typedef struct ID3CacheItem
{
	sds path;
	uint32_t mod_time;
	bool seen;

	// The ID3 fields
	sds artist;
	sds album;
	sds title;
	int32_t release_date; //days since 1970

	uint32_t track;
	float length;

	// Fields for rbtree
	char color_field;
	struct ID3CacheItem* left;
	struct ID3CacheItem* right;
} ID3CacheItem;

#define ID3CACHE_CMPARATOR( x, y ) strcmp( ( x )->path, ( y )->path )

SGLIB_DEFINE_RBTREE_PROTOTYPES( ID3CacheItem, left, right, color_field, ID3CACHE_CMPARATOR )

typedef struct ID3Cache
{
	mpg123_handle* mh;
	ID3CacheItem* root;
	const char* path;
	bool dirty;
} ID3Cache;

int id3_cache_new( ID3Cache** cache, const char* cache_path, mpg123_handle* mh );
int id3_cache_get( ID3Cache* cache,
				   const char* library_path,
				   const char* path,
				   ID3CacheItem** item );
int id3_cache_save( ID3Cache* cache );
