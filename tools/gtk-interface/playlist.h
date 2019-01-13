#ifndef _PLAYLIST_H_
#define _PLAYLIST_H_

#include <glib.h>
#include <gtk/gtk.h>

typedef struct playlist_item {
	GString *path;
	GString *artist;
	GString *album;
	GString *track;
	int item_id;
} PlaylistItem;

typedef struct playlist {
	GString *name;
	PlaylistItem *items;
	int num_items;
	GtkListStore *list_store;
} Playlist;

typedef struct playlists {
	Playlist *playlists;
	int num_playlists;
} Playlists;

int fetch_playlists( const char *endpoint, Playlists *playlists );

int add_playlist( const char *name, Playlists *playlists );

int send_playlist( const char *endpoint, const char *playlist_name, const Playlist *playlist );

int play_song( const char *endpoint, const char *playlist_name, int track_index, const char *when );

int music_pause( const char *endpoint );

const PlaylistItem* get_playlist_item_by_id( const Playlists *playlists, int *playlist_index, int item_id );

#endif // _PLAYLIST_H_

