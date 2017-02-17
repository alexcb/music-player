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
	(*playlist)->ref_count = 1;

	(*playlist)->name = sdsnew( name );
	if( (*playlist)->name == NULL ) {
		free( *playlist );
		return OUT_OF_MEMORY;
	}

	(*playlist)->root = NULL;
	(*playlist)->current = NULL;

	return OK;
}

int playlist_ref_up( Playlist *playlist )
{
	playlist->ref_count++;
	return 0;
}
int playlist_ref_down( Playlist *playlist )
{
	playlist->ref_count--;
	if( playlist->ref_count > 0 ) {
		return 0;
	}

	playlist_clear( playlist );
	sdsfree( playlist->name );
	free( playlist );
	return 0;
}

int playlist_rename( Playlist *playlist, const char *name )
{
	playlist->name = sdscpy( playlist->name, name );
	if( playlist->name == NULL ) {
		return OUT_OF_MEMORY;
	}

	return OK;
}

mpg123_handle *metadata_reader = NULL;
int read_metadata( const char *path, sds *artist, sds *title)
{
	mpg123_id3v1 *v1;
	mpg123_id3v2 *v2;

	if( metadata_reader == NULL ) {
		metadata_reader = mpg123_new( NULL, NULL );
	}

	if( mpg123_open( metadata_reader, path ) != MPG123_OK ) {
		LOG_ERROR( "path=s failed to open file", path );
		return UNKNOWN_ERROR;
	}

	mpg123_seek( metadata_reader, 0, SEEK_SET );
	if( mpg123_id3( metadata_reader, &v1, &v2 ) == MPG123_OK ) {
		// TODO use v2 if available
		if( v1 != NULL ) {
			*artist = sdscpy( *artist, v1->artist );
			*title = sdscpy( *title, v1->title );
		}
	}

	mpg123_close( metadata_reader );

	return OK;
}

int playlist_add_file( Playlist *playlist, const char *path )
{
	PlaylistItem *item = (PlaylistItem*) malloc( sizeof(PlaylistItem) );
	item->path = sdsnew( path );
	if( item->path == NULL ) {
		return OUT_OF_MEMORY;
	}
	item->ref_count = 1;
	item->artist = sdsempty();
	item->title = sdsempty();
	read_metadata( path, &item->artist, &item->title );

	item->next = NULL;
	item->parent = playlist;

	// TODO add a pointer to the last song in addition to root
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
	playlist_item_ref_down( item );
	return 0;
}

int playlist_item_ref_up( PlaylistItem *item )
{
	item->ref_count++;
	return 0;
}

int playlist_item_ref_down( PlaylistItem *item )
{
	item->ref_count--;
	if( item->ref_count > 0 ) {
		return 0;
	}

	sdsfree( item->path );
	sdsfree( item->artist );
	sdsfree( item->title );

	free( item );
	return 0;
}

int playlist_clear( Playlist *playlist )
{
	PlaylistItem *next;
	PlaylistItem *x = playlist->root;
	while( x ) {
		next = x->next;
		playlist_item_ref_down( x );
		x = next;
	}
	
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
