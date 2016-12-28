#include "playlist.h"

#include "errors.h"

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

    if (pthread_mutex_init(&(*playlist)->lock, NULL) != 0) {
		free( (*playlist)->name );
		free( *playlist );
		return MUTEX_ERROR;
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
	printf( "opening path %s\n", path );
	*fd = open( path, O_RDONLY );
	if( *fd < 0 ) {
		return 1;
	}
	return 0;
}

int open_stream( const char *url, int *fd )
{
	return 1;
}


int playlist_open_fd( Playlist *playlist, int *fd )
{
	int res;
	if( playlist->len == 0 ) {
		return 1;
	}
	assert( playlist->current < playlist->len );

	PlaylistItem *current = &playlist->list[playlist->current];
	
	printf("opening %s\n", current->path);
	if( strstr(current->path, "http://") ) {
		res = open_stream( current->path, fd );
	} else {
		res = open_file( current->path, fd );
	}

	playlist->current++;
	if( playlist->current > playlist->len ) {
		playlist->current = 0;
	}

	return res;
}

int playlist_next( Playlist *playlist )
{
    //int res = pthread_mutex_lock( &playlist->lock );
	//if( res ) {
	//	return res;
	//}

	//if( playlist->root == NULL ) {
	//	printf("playlist is empty\n");
    //	pthread_mutex_unlock( &playlist->lock );
	//	return 1;
	//}

	//if( playlist->current == NULL ) {
	//	printf("no current playlist file -- this should be set when playlist is loaded\n");
    //	pthread_mutex_unlock( &playlist->lock );
	//	return 1;
	//}

	//if( playlist->current->next == NULL ) {
	//	playlist->current = playlist->root;
    //	pthread_mutex_unlock( &playlist->lock );
	//	return 0;
	//}

	//playlist->current = playlist->current->next;
    //pthread_mutex_unlock( &playlist->lock );
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
