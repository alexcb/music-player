#pragma once

#include "playlist.h"

#include <stdio.h>

typedef struct PlaylistManager
{
	Playlist *playlists[1024];
	int len;
	int current;
	pthread_mutex_t lock;
} PlaylistManager;

int playlist_manager_init( PlaylistManager *manager );

int load_quick_album( PlaylistManager *manager, const char *path );

int playlist_manager_open_fd( PlaylistManager *manager, int *fd );
