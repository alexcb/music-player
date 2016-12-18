#include "playlist.h"

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


typedef enum {
	PLAYLIST_OK = 0,
	PLAYLIST_OUT_OF_MEMORY,
	PLAYLIST_MUTEX
} PlaylistError;


int playlist_new( Playlist **playlist, const char *name )
{
	*playlist = (Playlist*) malloc(sizeof(Playlist));
	if( *playlist == NULL ) {
		return PLAYLIST_OUT_OF_MEMORY;
	}

	(*playlist)->name = strdup( name );
	if( (*playlist)->name == NULL ) {
		free( *playlist );
		return PLAYLIST_OUT_OF_MEMORY;
	}

    if (pthread_mutex_init(&(*playlist)->lock, NULL) != 0) {
		free( (*playlist)->name );
		free( *playlist );
		return PLAYLIST_MUTEX;
	}

	(*playlist)->last = (*playlist)->root = (*playlist)->current = NULL;
	return PLAYLIST_OK;
}

int playlist_add_file( Playlist *playlist, const char *path )
{
	PlaylistItem *item = (PlaylistItem*) malloc( sizeof(PlaylistItem) );
	if( !item ) {
		return PLAYLIST_OUT_OF_MEMORY;
	}
	item->next = NULL;
	item->path = strdup( path );
	if( !item->path ) {
		free( item );
		return PLAYLIST_OUT_OF_MEMORY;
	}

    pthread_mutex_lock( &playlist->lock );
	if( playlist->last ) {
		playlist->last->next = item;
		playlist->last = item;
	} else {
		playlist->current = playlist->last = playlist->root = item;
	}

    pthread_mutex_unlock( &playlist->lock );
	return PLAYLIST_OK;
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


int playlist_open_current_fd( Playlist *playlist, int *fd )
{
	int res;
    pthread_mutex_lock( &playlist->lock );

	if( playlist->current == NULL ) {
		printf("no current playlist file\n");
		return 1;
	}
	
	printf("opening %s\n", playlist->current->path);
	if( strstr(playlist->current->path, "http://") ) {
		res = open_stream( playlist->current->path, fd );
	} else {
		res = open_file( playlist->current->path, fd );
	}

    pthread_mutex_unlock( &playlist->lock );
	return res;
}

int playlist_next( Playlist *playlist )
{
    pthread_mutex_lock( &playlist->lock );

	if( playlist->root == NULL ) {
		printf("playlist is empty\n");
    	pthread_mutex_unlock( &playlist->lock );
		return 1;
	}

	if( playlist->current == NULL ) {
		printf("no current playlist file -- this should be set when playlist is loaded\n");
    	pthread_mutex_unlock( &playlist->lock );
		return 1;
	}

	if( playlist->current->next == NULL ) {
		playlist->current = playlist->root;
    	pthread_mutex_unlock( &playlist->lock );
		return 0;
	}

	playlist->current = playlist->current->next;
    pthread_mutex_unlock( &playlist->lock );
	return 0;
}

