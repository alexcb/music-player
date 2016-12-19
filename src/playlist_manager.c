#include "playlist_manager.h"

#include "string_utils.h"

#include <dirent.h> 
#include <string.h>

int playlist_manager_init( PlaylistManager *manager )
{
	int res;

    if( pthread_mutex_init( &manager->lock, NULL ) != 0 ) {
		return 1;
	}

	res = playlist_new( &manager->playlists[0], "Quick Album" );
	if( res != 0 ) {
		return res;
	}

	res = playlist_new( &manager->playlists[1], "kexp" );
	if( res != 0 ) {
		return res;
	}
	res = playlist_add_file( manager->playlists[1], "http://live-mp3-128.kexp.org:80/kexp128.mp3" );
	if( res != 0 ) {
		return res;
	}

	res = playlist_new( &manager->playlists[2], "kcrw" );
	if( res != 0 ) {
		return res;
	}
	res = playlist_add_file( manager->playlists[2], "http://kcrw.streamguys1.com/kcrw_192k_mp3_e24_internet_radio" );
	if( res != 0 ) {
		return res;
	}

	manager->current = 0;
	manager->len = 3;
	return 0;
}

int load_quick_album_recursive( PlaylistManager *manager, const char *path )
{
	int res;
	char p[1024];
	struct dirent *ent;

	DIR *d = opendir( path );
	if( d == NULL ) {
		return 1;
	}

	while( (ent = readdir(d)) != NULL) {
		sprintf(p, "%s/%s", path, ent->d_name);
		if( ent->d_type == DT_REG && has_suffix(p, ".mp3") ) {
			res = playlist_add_file( manager->playlists[0], p );
			if( res ) {
				closedir( d );
				return res;
			}
		} else if( ent->d_type == DT_DIR && strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0 ) {
			res = load_quick_album_recursive( manager, p );
			if( res ) {
				closedir( d );
				return res;
			}
		}
	}
	closedir( d );
	return 0;
}

int load_quick_album( PlaylistManager *manager, const char *path )
{
	int res;
    res = pthread_mutex_lock( &manager->lock );
	if( res ) {
		return res;
	}

	res = playlist_clear( manager->playlists[0] );
	if( res ) {
    	pthread_mutex_unlock( &manager->lock );
		return res;
	}

	res = load_quick_album_recursive( manager, path );
    pthread_mutex_unlock( &manager->lock );
	return res;
}

int playlist_manager_open_fd( PlaylistManager *manager, int *fd )
{
	int res;
    res = pthread_mutex_lock( &manager->lock );
	if( res ) {
		return res;
	}

	if( 0 <= manager->current && manager->current < manager->len ) {
		res = playlist_open_fd( manager->playlists[manager->current], fd );
	} else {
		res = 1;
	}

    pthread_mutex_unlock( &manager->lock );
	return res;
}
