#pragma once

#include <pthread.h>

typedef struct PlaylistItem
{
	char *path;
} PlaylistItem;

typedef struct Playlist
{
	char *name;
	int len;
	int cap;
	int current;
	PlaylistItem *list;
	//PlaylistItem *root;
	//PlaylistItem *last;
	//PlaylistItem *current;
	pthread_mutex_t lock;
} Playlist;

int playlist_new( Playlist **playlist, const char *name );
int playlist_clear( Playlist *playlist );
int playlist_add_file( Playlist *playlist, const char *path );
int playlist_open_fd( Playlist *playlist, int *fd, long int *icy_interval );
int playlist_next( Playlist *playlist );
void playlist_sort_by_path( Playlist *playlist );
