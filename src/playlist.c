#include "playlist.h"

#include "errors.h"
#include "log.h"
#include "httpget.h"

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <assert.h>


int playlist_new( Playlist **playlist, const char *name )
{
	*playlist = (Playlist*) malloc(sizeof(Playlist));
	if( *playlist == NULL ) {
		return OUT_OF_MEMORY;
	}

	(*playlist)->name = strdup( name );
	if( (*playlist)->name == NULL ) {
		free( *playlist );
		return OUT_OF_MEMORY;
	}

	(*playlist)->len = 0;
	(*playlist)->cap = 128;
	(*playlist)->current = 0;
	(*playlist)->list = (PlaylistItem*) malloc(sizeof(PlaylistItem) * (*playlist)->cap);
	if( (*playlist)->list == NULL ) {
		return OUT_OF_MEMORY;
	}

	return OK;
}

int playlist_rename( Playlist *playlist, const char *name )
{
	free( playlist->name );
	playlist->name = strdup( name );
	if( playlist->name == NULL ) {
		return OUT_OF_MEMORY;
	}

	return OK;
}

int playlist_grow( Playlist *playlist )
{
	if( playlist->len == playlist->cap ) {
		size_t new_cap = playlist->cap * 2;
		PlaylistItem *new_list = (PlaylistItem*) realloc(playlist->list, sizeof(PlaylistItem) * new_cap);
		if( new_list == NULL ) {
			return OUT_OF_MEMORY;
		}
		playlist->list = new_list;
		playlist->cap = new_cap;
	}
	return 0;
}

int playlist_add_file( Playlist *playlist, const char *path )
{
	int res;
	res = playlist_grow( playlist );
	if( res ) {
		return res;
	}
	PlaylistItem *item = &playlist->list[playlist->len];
	item->path = strdup( path );
	if( !item->path ) {
		return OUT_OF_MEMORY;
	}

	playlist->len++;
	return OK;
}

int open_file( const char *path, int *fd )
{
	LOG_DEBUG( "path=s open_file", path );
	*fd = open( path, O_RDONLY );
	if( *fd < 0 ) {
		return 1;
	}
	return 0;
}


// TODO does this need to be global? can it be in the player instead?
struct httpdata hd;
int open_stream( const char *url, int *fd, long int *icy_interval )
{
	LOG_DEBUG( "url=s open_stream", url );
	*fd = http_open(url, &hd);
	*icy_interval = hd.icy_interval;
	//printf("setting icy %ld\n", hd.icy_interval);
	//if(MPG123_OK != mpg123_param(mh, MPG123_ICY_INTERVAL, hd.icy_interval, 0)) {
	//	printf("unable to set icy interval\n");
	//	return 1;
	//}
	//if(mpg123_open_fd(mh, *fd) != MPG123_OK) {
	//	printf("error\n");
	//	return 1;
	//}

	return 0;
}


int playlist_open_fd( Playlist *playlist, int *fd, long int *icy_interval )
{
	int res;
	if( playlist->len == 0 ) {
		return 1;
	}
	assert( playlist->current < playlist->len );

	PlaylistItem *current = &playlist->list[playlist->current];
	
	if( strstr(current->path, "http://") ) {
		res = open_stream( current->path, fd, icy_interval );
	} else {
		res = open_file( current->path, fd );
		*icy_interval = 0;
	}

	playlist_next( playlist );
	return res;
}

int playlist_next( Playlist *playlist )
{
	playlist->current++;
	if( playlist->current >= playlist->len ) {
		playlist->current = 0;
	}
	return 0;
}

int playlist_clear( Playlist *playlist )
{
	for( int i = 0; i < playlist->len; i++ ) {
		free( playlist->list[i].path );
	}

	playlist->len = 0;
	playlist->current = 0;

	return 0;
}

int playlist_item_cmp( const void *a, const void *b )
{
	const PlaylistItem *aa = (const PlaylistItem*) a;
	const PlaylistItem *bb = (const PlaylistItem*) b;
	return strcmp( aa->path, bb->path );
}

void playlist_sort_by_path( Playlist *playlist )
{
	qsort( playlist->list, playlist->len, sizeof(PlaylistItem), playlist_item_cmp);
}
