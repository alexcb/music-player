
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
	item->track = 0; //TODO stored in comment[30] of id3

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

int read_str( FILE *fp, sds *s )
{
	int res;
	int n;
	res = fread( &n, sizeof(int), 1, fp );
	if( res != 1 ) {
		return 1;
	}
	if( n < 0 ) {
		*s = NULL;
		return 0;
	}
	*s = sdsnewlen( fp, n+1 );
	fread( *s, 1, n, fp );
	sdsrange( *s, 0, n-1 );
	return 0;
}

int id3_cache_load( ID3Cache *cache )
{
	int res;
	LOG_INFO( "path=s saving id3 cache", cache->path );
	FILE *fp = fopen ( cache->path, "rb" );
	if( !fp ) {
		LOG_ERROR( "path=s failed to open library for reading", cache->path );
		return 1;
	}
	// Versioning
	sds version;
	res = read_str( fp, &version );
	if( res ) {
		LOG_ERROR( "unable to read version" );
	}
	printf("got -%s-\n", version);
	sdsfree(version);

	ID3CacheItem *item;

	for(;;) {
		item = (ID3CacheItem*) malloc(sizeof(ID3CacheItem));
		res = read_str( fp, &item->path);
		if( res ) {
			free(item);
			break;
		}

		res = read_str( fp, &item->album  ); if( res ) { LOG_ERROR( "unable to read complete record" ); break; }
		res = read_str( fp, &item->artist ); if( res ) { LOG_ERROR( "unable to read complete record" ); break; }
		res = read_str( fp, &item->title  ); if( res ) { LOG_ERROR( "unable to read complete record" ); break; }
		
		LOG_INFO( "path=s adding cache entry", item->path );
		sglib_ID3CacheItem_add( &(cache->root), item );
	}
	LOG_INFO( "path=s done reading cache", cache->path );

	fclose( fp );
	return 0;
}

int id3_cache_new( ID3Cache **cache, const char *path, mpg123_handle *mh )
{
	*cache = (ID3Cache*) malloc(sizeof(ID3Cache));
	(*cache)->root = NULL;
	(*cache)->mh = mh;
	(*cache)->path = path;

	id3_cache_load( *cache );
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
		memset( *item, 0, sizeof(ID3CacheItem) );
		res = id3_get( cache, path, *item );
		if( res ) {
			LOG_INFO( "path=s failed to read mp3 id3", path );
			free( *item );
			return 1;
		}
		LOG_INFO( "path=s adding done", cache->path );
		sglib_ID3CacheItem_add( &(cache->root), *item );
	}
	(*item)->seen = true;
	return 0;
}

int id3_cache_add( ID3Cache *cache, const char *path )
{
	ID3CacheItem *item;
	return id3_cache_get( cache, path, &item );
}

void write_str( FILE *fp, const char *s )
{
	int n = -1;
	if( s ) {
		n = strlen( s );
	}
	fwrite( &n, sizeof(int), 1, fp );
	if( n > 0 ) {
		fwrite( s, 1, n, fp );
	}
}

int id3_cache_save( ID3Cache *cache )
{
	LOG_INFO( "path=s saving id3 cache", cache->path );
	FILE *fp = fopen ( cache->path, "wb" );
	if( !fp ) {
		LOG_ERROR( "path=s failed to open library for writing", cache->path );
		return 1;
	}
	// Versioning
	LOG_INFO( "saving verson" );
	write_str( fp, "a" );

	struct sglib_ID3CacheItem_iterator it;
	ID3CacheItem *te;
	for( te=sglib_ID3CacheItem_it_init_inorder(&it, cache->root); te!=NULL; te=sglib_ID3CacheItem_it_next(&it) ) {
		LOG_INFO( "x=s saving item path", te->path );
		write_str( fp, te->path );
		LOG_INFO( "x=s saving item album", te->album );
		write_str( fp, te->album );
		LOG_INFO( "x=s saving item artist", te->artist );
		write_str( fp, te->artist );
		LOG_INFO( "x=s saving item title", te->title );
		write_str( fp, te->title );
	}
	LOG_INFO( "path=s done saving id3 cache", cache->path );

	fclose( fp );
	return 0;

}
