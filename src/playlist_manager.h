#pragma once

#include "albums.h"
#include "playlist.h"
#include "sds.h"

#include <stdio.h>

typedef struct PlaylistManager
{
	Playlist *root;
	Library *library;
	sds playlistPath;
	pthread_mutex_t lock;
} PlaylistManager;

int playlist_manager_init( PlaylistManager *manager, const char *path, Library *library );

int playlist_manager_new_playlist( PlaylistManager *manager, const char *name, Playlist **p );
int playlist_manager_get_playlist( PlaylistManager *manager, const char *name, Playlist **p );
int playlist_manager_delete_playlist( PlaylistManager *manager, const char *name );

int playlist_manager_save( PlaylistManager *manager );
int playlist_manager_load( PlaylistManager *manager );

int playlist_manager_get_item_by_id( PlaylistManager *manager, int id, PlaylistItem **i );

int playlist_manager_checksum( PlaylistManager *manager );
