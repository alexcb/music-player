#pragma once

#include <pthread.h>
#include "circular_buffer.h"

typedef struct PlaylistItem
{
	char *path;
	struct PlaylistItem *next;
	struct PlaylistItem *prev;
} PlaylistItem;

typedef struct Playlist
{
	char *name;
	int len;
	int cap;
	int current;
	PlaylistItem *root;
	struct Playlist *next;
	struct Playlist *prev;
} Playlist;

int playlist_new( Playlist **playlist, const char *name );
int playlist_rename( Playlist *playlist, const char *name );
int playlist_clear( Playlist *playlist );
int playlist_add_file( Playlist *playlist, const char *path );
int playlist_remove_item( Playlist *playlist, PlaylistItem *item );
void playlist_sort_by_path( Playlist *playlist );

int open_file( const char *path, int *fd );
