
#include "id3.h"
#include "log.h"

#include "player.h"
#include "icy.h"
#include "log.h"
#include "io_utils.h"

#include <stdio.h>
#include <stdbool.h>
#include <mpg123.h>



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

	item->path = sdsnew( path );

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
				if( v2->artist && v2->artist->p)
					item->artist = sdsnew( v2->artist->p );
				if( v2->album && v2->album->p)
					item->album = sdsnew( v2->album->p );
				if( v2->title && v2->title->p)
					item->title = sdsnew( v2->title->p );
			} else if( v1 != NULL ) {
				LOG_DEBUG( "populating metadata with id3 v1" );
				item->artist = sdsnew( v1->artist );
				item->title = sdsnew( v1->title );
				item->album = sdsnew( v1->album );
			} else {
				assert( false );
			}
		}
	}

error:
	mpg123_close( cache->mh );
	return res;
}

int id3_cache_new( ID3Cache **cache, const char *path, mpg123_handle *mh )
{
	*cache = (ID3Cache*) malloc(sizeof(ID3Cache));
	(*cache)->root = NULL;
	(*cache)->mh = mh;
	(*cache)->path = path;
	return 0;
}

int id3_cache_get( ID3Cache *cache, const char *path, ID3CacheItem **item )
{
	LOG_INFO( "path=s cache_get", path );
	int res;
	ID3CacheItem id;
	id.path = (sds) path;
	*item = sglib_ID3CacheItem_find_member( cache->root, &id );
	if( *item == NULL ) {
		LOG_INFO( "path=s adding", path );
		*item = (ID3CacheItem*) malloc(sizeof(ID3CacheItem));
		res = id3_get( cache, path, *item );
		if( res ) {
			LOG_INFO( "path=s failed to read mp3 id3", path );
			free( *item );
			return 1;
		}
		LOG_INFO( "path=s adding done", cache->path );
		sglib_ID3CacheItem_add( &(cache->root), *item );
	}
	return 0;
}

int id3_cache_add( ID3Cache *cache, const char *path )
{
	ID3CacheItem *item;
	return id3_cache_get( cache, path, &item );
}

void write_str( FILE *fp, const char *s )
{
	int n = strlen( s );
	fwrite( &n, sizeof(int), 1, fp );
	fwrite( s, 1, n, fp );
}

int id3_cache_save( ID3Cache *cache )
{
	LOG_INFO( "path=s saving id3 cache", cache->path );
	FILE *fp = fopen ( cache->path, "wb" );
	if( !fp ) {
		LOG_ERROR( "path=s failed to open library for writing", cache->path );
		return 1;
	}
	struct sglib_ID3CacheItem_iterator it;
	ID3CacheItem *te;
	for( te=sglib_ID3CacheItem_it_init_inorder(&it, cache->root); te!=NULL; te=sglib_ID3CacheItem_it_next(&it) ) {
		printf("saving %s\n", te->path);
		write_str( fp, te->path );
		write_str( fp, te->album );
		write_str( fp, te->artist );
		write_str( fp, te->title );
	}
	LOG_INFO( "path=s done saving id3 cache", cache->path );

	fclose( fp );
	return 0;

}
