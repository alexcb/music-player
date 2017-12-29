#pragma once

#include <pthread.h>
#include "circular_buffer.h"
#include "sds.h"
#include <stdbool.h>

struct Track;
struct Playlist;

typedef struct PlaylistItem
{
	int id;
	int ref_count;
	const struct Track *track;
	bool is_stream;
	struct PlaylistItem *next;
	struct PlaylistItem *prev;
	struct Playlist *parent;
} PlaylistItem;

typedef struct Playlist
{
	int id;
	int ref_count;
	sds name;
	PlaylistItem *root;
	PlaylistItem *current;
	struct Playlist *next;
	struct Playlist *prev;
} Playlist;

int playlist_new( Playlist **playlist, const char *name );
int playlist_rename( Playlist *playlist, const char *name );
int playlist_clear( Playlist *playlist );
int playlist_update( Playlist *playlist, PlaylistItem *item );
int playlist_add_file( Playlist *playlist, const struct Track *track, int track_id );
int playlist_remove_item( Playlist *playlist, PlaylistItem *item );
void playlist_sort_by_path( Playlist *playlist );

int playlist_ref_up( Playlist *playlist );
int playlist_ref_down( Playlist *playlist );

int playlist_item_ref_up( PlaylistItem *item );
int playlist_item_ref_down( PlaylistItem *item );

int open_file( const char *path, int *fd );
