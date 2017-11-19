
#include "id3.h"
#include "log.h"

#include "player.h"
#include "icy.h"
#include "log.h"
#include "io_utils.h"

#include <stdio.h>
#include <stdbool.h>
#include <sys/stat.h>

#include <mpg123.h>



SGLIB_DEFINE_RBTREE_FUNCTIONS(ID3CacheItem, left, right, color_field, ID3CACHE_CMPARATOR)

int id3_get( ID3Cache *cache, const char *path, ID3CacheItem *item )
{
	int res;
	LOG_DEBUG( "path=s reading id3 tags", path );
	res = mpg123_open( cache->mh, path );
	if( res != MPG123_OK ) {
		LOG_ERROR("err=d open error", res);
		res = 1;
		goto error;
	}
	mpg123_seek( cache->mh, 0, SEEK_SET );

	item->path = sdsnew( path );
	item->track = 0; //TODO stored in comment[30] of id3

	mpg123_id3v1 *v1;
	mpg123_id3v2 *v2;
	res = 1;
	int meta = mpg123_meta_check( cache->mh );
	if( meta & MPG123_NEW_ID3 ) {
		if( mpg123_id3( cache->mh, &v1, &v2 ) == MPG123_OK ) {
			res = 0;
			if( v2 != NULL ) {
				if( v2->artist && v2->artist->p)
					item->artist = sdsnew( v2->artist->p );
				if( v2->album && v2->album->p)
					item->album = sdsnew( v2->album->p );
				if( v2->title && v2->title->p)
					item->title = sdsnew( v2->title->p );
			} else if( v1 != NULL ) {
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

int read_long( FILE *fp, long *x )
{
	int res;
	res = fread( x, sizeof(long), 1, fp );
	if( res != 1 ) {
		return 1;
	}
	return 0;
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
	LOG_INFO( "path=s loading id3 cache", cache->path );
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

		res = read_long( fp, &item->mod_time ); if( res ) { LOG_ERROR( "unable to read complete record" ); break; }
		res = read_str( fp, &item->album     ); if( res ) { LOG_ERROR( "unable to read complete record" ); break; }
		res = read_str( fp, &item->artist    ); if( res ) { LOG_ERROR( "unable to read complete record" ); break; }
		res = read_str( fp, &item->title     ); if( res ) { LOG_ERROR( "unable to read complete record" ); break; }
		
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
	(*cache)->dirty = false;

	id3_cache_load( *cache );
	return 0;
}

int id3_cache_get( ID3Cache *cache, const char *path, ID3CacheItem **item )
{
	struct stat st;

	if (stat(path, &st)) {
		LOG_ERROR("path=s failed to stat file (caching will be disabled)", path);
		st.st_mtim.tv_sec = 0;
	}

	int res;
	ID3CacheItem id;
	id.path = (sds) path;
	*item = sglib_ID3CacheItem_find_member( cache->root, &id );
	if( *item == NULL ) {
		LOG_INFO( "path=s item was not found in cache", path );
		*item = (ID3CacheItem*) malloc(sizeof(ID3CacheItem));
		memset( *item, 0, sizeof(ID3CacheItem) );
		(*item)->mod_time = st.st_mtim.tv_sec;
		res = id3_get( cache, path, *item );
		if( res ) {
			LOG_INFO( "path=s failed to read mp3 id3", path );
			free( *item );
			return 1;
		}
		sglib_ID3CacheItem_add( &(cache->root), *item );
		cache->dirty = true;
	}
	if( (*item)->mod_time != st.st_mtim.tv_sec ) {
		LOG_INFO(
			"path=s cachemtime=d mtime=d mod time doesnt match cached version, re-reading",
			cache->path, (*item)->mod_time, st.st_mtim.tv_sec
			);
		res = id3_get( cache, path, *item );
		if( res ) {
			LOG_INFO( "path=s failed to read mp3 id3", path );
			free( *item );
			return 1;
		}
		(*item)->mod_time = st.st_mtim.tv_sec;
		cache->dirty = true;
	}
	(*item)->seen = true;
	return 0;
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

void write_long( FILE *fp, long x )
{
	fwrite( &x, sizeof(long), 1, fp );
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
	write_str( fp, "a" );

	struct sglib_ID3CacheItem_iterator it;
	ID3CacheItem *te;
	for( te=sglib_ID3CacheItem_it_init_inorder(&it, cache->root); te!=NULL; te=sglib_ID3CacheItem_it_next(&it) ) {
		write_str( fp, te->path );
		fwrite( &(te->mod_time), sizeof(long), 1, fp );
		write_str( fp, te->album );
		write_str( fp, te->artist );
		write_str( fp, te->title );
	}
	LOG_INFO( "path=s done saving id3 cache", cache->path );

	fclose( fp );
	return 0;

}
