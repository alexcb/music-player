#include <stdbool.h>
#include <mpg123.h>

#include "id3.h"
#include "log.h"

#include "player.h"
#include "icy.h"
#include "log.h"
#include "io_utils.h"

#include <stdio.h>



SGLIB_DEFINE_RBTREE_FUNCTIONS(ID3CacheItem, left, right, color_field, ID3CACHE_CMPARATOR)

int id3_get( ID3Cache *cache, const char *path, ID3CacheItem *item )
{
	int res;
	LOG_DEBUG( "path=s open", path );
	res = mpg123_open( cache->mh, path );
	if( res != MPG123_OK ) {
		LOG_DEBUG("err=d open error", res);
		res = 1;
		goto error;
	}
	LOG_DEBUG( "seek" );
	mpg123_seek( cache->mh, 0, SEEK_SET );

	LOG_DEBUG( "reading id3" );
	mpg123_id3v1 *v1;
	mpg123_id3v2 *v2;
	res = 1;
	int meta = mpg123_meta_check( cache->mh );
	if( meta & MPG123_NEW_ID3 ) {
		if( mpg123_id3( cache->mh, &v1, &v2 ) == MPG123_OK ) {
			res = 0;
			if( v2 != NULL ) {
				LOG_DEBUG( "populating metadata with id3 v2" );
				if( v2->artist )
					strcpy( item->artist, v2->artist->p );
				if( v2->album )
					strcpy( item->album, v2->album->p );
				if( v2->title )
					strcpy( item->title, v2->title->p );
			} else if( v1 != NULL ) {
				LOG_DEBUG( "populating metadata with id3 v1" );
				strcpy( item->artist, v1->artist );
				strcpy( item->title, v1->title );
				strcpy( item->album, v1->album );
			} else {
				assert( false );
			}
		}
	}

error:
	mpg123_close( cache->mh );
	return res;
}

int id3_cache_new( ID3Cache **cache, mpg123_handle *mh )
{
	*cache = (ID3Cache*) malloc(sizeof(ID3Cache));
	(*cache)->root = NULL;
	(*cache)->mh = mh;
	return 0;
}

int id3_cache_get( ID3Cache *cache, const char *path, ID3CacheItem **item )
{
	int res;
	ID3CacheItem id;
	id.path = (char*) path;
	*item = sglib_ID3CacheItem_find_member( cache->root, &id );
	if( item == NULL ) {
		*item = (ID3CacheItem*) malloc(sizeof(ID3CacheItem));
		res = id3_get( cache, path, *item );
		if( !res ) {
			free( *item );
			return 1;
		}
		sglib_ID3CacheItem_add( &(cache->root), *item );
	}
	return 0;
}

