#include <pthread.h>

typedef struct PlaylistItem
{
	const char *path;
	struct PlaylistItem *next;
} PlaylistItem;

typedef struct Playlist
{
	char *name;
	PlaylistItem *root;
	PlaylistItem *last;
	PlaylistItem *current;
	pthread_mutex_t lock;
} Playlist;

int playlist_new( Playlist **playlist, const char *name );
int playlist_add_file( Playlist *playlist, const char *path );
int playlist_open_current_fd( Playlist *playlist, int *fd );
int playlist_next( Playlist *playlist );
