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

	(*playlist)->root = NULL;

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

int playlist_add_file( Playlist *playlist, const char *path )
{
	PlaylistItem *item = (PlaylistItem*) malloc( sizeof(PlaylistItem) );
	item->path = strdup( path );
	item->next = NULL;
	item->parent = playlist;

	PlaylistItem *prev = NULL;
	PlaylistItem **p = &(playlist->root);
	while( *p != NULL ) {
		prev = *p;
		p = &((*p)->next);
	}

	*p = item;
	item->prev = prev;

	return OK;
}

int playlist_remove_item( Playlist *playlist, PlaylistItem *item )
{
	if( item->prev ) {
		item->prev->next = item->next;
	}
	if( item->next ) {
		item->next->prev = item->prev;
	}
	return 0;
}

int playlist_clear( Playlist *playlist )
{
	//TODO massive memory leak
	playlist->root = NULL;

	return 0;
}

//int playlist_item_cmp( const void *a, const void *b )
//{
//	const PlaylistItem *aa = (const PlaylistItem*) a;
//	const PlaylistItem *bb = (const PlaylistItem*) b;
//	return strcmp( aa->path, bb->path );
//}

void playlist_sort_by_path( Playlist *playlist )
{
	//qsort( playlist->list, playlist->len, sizeof(PlaylistItem), playlist_item_cmp);
}
