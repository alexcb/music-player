#pragma once

#include "playlist.h"

#include <stdio.h>

typedef struct PlaylistManager
{
	Playlist *playlists[1024];
	int len;
	int current;
	volatile int version;
	pthread_mutex_t lock;
} PlaylistManager;

int playlist_manager_init( PlaylistManager *manager );

void playlist_manager_lock( PlaylistManager *manager );
void playlist_manager_unlock( PlaylistManager *manager );

int playlist_manager_get_version( PlaylistManager *manager, int *version );
int playlist_manager_get_path( PlaylistManager *manager, int index, const char **path );
int playlist_manager_get_length( PlaylistManager *manager, int *len );


int load_quick_album( PlaylistManager *manager, const char *path );

int playlist_manager_open_fd( PlaylistManager *manager, int *fd, long int *icy_interval, char **playlist );

int playlist_manager_next( PlaylistManager *manager );
int playlist_manager_prev( PlaylistManager *manager );
