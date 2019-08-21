#include "playlist.h"

#include "errors.h"
#include "log.h"
#include "httpget.h"
#include "library.h"
#include "my_malloc.h"

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <assert.h>

int playlist_id_next = 1;

int playlist_new( Playlist **playlist, const char *name )
{
	*playlist = (Playlist*) my_malloc(sizeof(Playlist));
	if( *playlist == NULL ) {
		return OUT_OF_MEMORY;
	}
	(*playlist)->ref_count = 1;

	(*playlist)->name = sdsnew( name );
	if( (*playlist)->name == NULL ) {
		my_free( *playlist );
		return OUT_OF_MEMORY;
	}

	(*playlist)->root = NULL;
	(*playlist)->current = NULL;
	(*playlist)->id = playlist_id_next++;

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
	my_free( playlist );
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

int playlist_add_file( Playlist *playlist, const Track *track, int track_id )
{
	assert(0);
//	PlaylistItem *item = (PlaylistItem*) my_malloc( sizeof(PlaylistItem) );
//	item->track = track;
//	item->ref_count = 1;
//
//	item->next = NULL;
//	item->parent = playlist;
//
//	PlaylistItem **p = &(playlist->root);
//	while( *p != NULL ) {
//		p = &((*p)->next);
//	}
//
//	*p = item;
//	if( track_id > 0 ) {
//		item->id = track_id;
//	} else {
//		item->id = playlist_id_next++;
//	}
//
	return OK;
}

int playlist_remove_item( Playlist *playlist, PlaylistItem *item )
{
	assert(0);
	//if( item->prev ) {
	//	item->prev->next = item->next;
	//}
	//if( item->next ) {
	//	item->next->prev = item->prev;
	//}
	//playlist_item_ref_down( item );
	return 0;
}

int playlist_item_ref_up( PlaylistItem *item )
{
	item->ref_count++;
	//LOG_DEBUG("path=s count=d playlist_item_ref_up", item->track->path, item->ref_count);
	return 0;
}

int playlist_item_ref_down( PlaylistItem *item )
{
	assert( item );
	assert( item->ref_count>0 );
	item->ref_count--;
	//LOG_DEBUG("path=s count=d playlist_item_ref_down", item->track->path, item->ref_count);
	if( item->ref_count > 0 ) {
		return 0;
	}

	//free( item );
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

PlaylistItem* pop_item_with_track_id( PlaylistItem **root, int track_id )
{
	PlaylistItem *x = *root;
	PlaylistItem **prev = root;
	while( x ) {
		if( x->id == track_id ) {
			*prev = x->next;
			return x;
		}
		prev = &(x->next);
		x = x->next;
	}
	return NULL;
}

// this function will steal the ref counts
int playlist_update( Playlist *playlist, PlaylistItem *item )
{
	PlaylistItem *old_root = playlist->root;
	PlaylistItem *last = NULL;
	PlaylistItem *p = NULL;
	PlaylistItem *n = NULL;

	LOG_DEBUG("playlist_update start");
	//p = old_root;
	//while( p ) {
	//	LOG_DEBUG("p=p path=s ref=d ref count", p, p->track->path, p->ref_count);
	//	p = p->next;
	//}

	PlaylistItem *x = item;
	while( x ) {
		n = x->next;

		p = NULL;
		if( x->id > 0 ) {
			p = pop_item_with_track_id( &old_root, x->id );
		}
		if( p == NULL ) {
			p = x; // steal the ref counter
			p->id = playlist_id_next++;
		} else {
			//discard x; and keep the original track item (p)
			playlist_item_ref_down( x );
		}

		p->parent = playlist;

		if( last == NULL ) {
			//LOG_DEBUG("p=p path=s id=d adding root", p, p->track->path, p->id);
			playlist->root = p;
		} else {
			last->next = p;
			//LOG_DEBUG("p=p path=s id=d adding next", p, p->track->path, p->id);
		}

		last = p;
		x = n;
	}

	// remove any left over tracks that weren't contained in the new list
	//there's a bug when I play files, then a stream, then back to files.
	//BUG p = old_root;
	//BUG while( p ) {
	//BUG 	x = p->next;
	//BUG 	if( p->track ) {
	//BUG 		LOG_DEBUG("p=p path=s ref=d deleting orphaned", p, p->track->path, p->ref_count);
	//BUG 	} else {
	//BUG 		LOG_DEBUG("p=p path=no-track(stream?) ref=d deleting orphaned", p, p->ref_count);
	//BUG 	}
	//BUG 	p->parent = NULL; // this track has been orphaned; there's a chance it's currently being played
	//BUG 	playlist_item_ref_down( p );
	//BUG 	p = x;
	//BUG }

	// update ID (to incidicate the playlist has changed)
	playlist->id = playlist_id_next++;

	//p = playlist->root;
	//LOG_DEBUG("after");
	//while( p ) {
	//	//LOG_DEBUG("p=p path=s ref=d ref count", p, p->track->path, p->ref_count);
	//	p = p->next;
	//}
	
	LOG_DEBUG("playlist_update done");
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

