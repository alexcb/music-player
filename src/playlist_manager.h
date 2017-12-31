#pragma once

#include "playlist.h"
#include "sds.h"

#include <stdio.h>

typedef struct PlaylistManager
{
	Playlist *root;
	sds playlistPath;
	pthread_mutex_t lock;
} PlaylistManager;

int playlist_manager_init( PlaylistManager *manager, const char *path );

//void playlist_manager_lock( PlaylistManager *manager );
//void playlist_manager_unlock( PlaylistManager *manager );

int playlist_manager_new_playlist( PlaylistManager *manager, const char *name, Playlist **p );
int playlist_manager_get_playlist( PlaylistManager *manager, const char *name, Playlist **p );
int playlist_manager_delete_playlist( PlaylistManager *manager, const char *name );

int load_quick_album( PlaylistManager *manager, const char *path );

int playlist_manager_save( PlaylistManager *manager );
int playlist_manager_load( PlaylistManager *manager );

int playlist_manager_get_item_by_id( PlaylistManager *manager, int id, PlaylistItem **i );
