#define _GNU_SOURCE // to get the gnu version of basename
#include "playlist_manager.h"

#include "string_utils.h"
#include "errors.h"
#include "log.h"

#include <dirent.h> 
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


int playlist_manager_init( PlaylistManager *manager, const char *path )
{
	int res;

    if( pthread_mutex_init( &manager->lock, NULL ) != 0 ) {
		return 1;
	}

	manager->playlistPath = sdsnew( path );

	//res = playlist_new( &manager->playlists[0], "Quick Album" );
	//if( res != 0 ) {
	//	return res;
	//}

	//res = playlist_new( &manager->playlists[1], "kexp" );
	//if( res != 0 ) {
	//	return res;
	//}
	//res = playlist_add_file( manager->playlists[1], "http://live-mp3-128.kexp.org:80/kexp128.mp3" );
	//if( res != 0 ) {
	//	return res;
	//}

	//res = playlist_new( &manager->playlists[2], "kcrw" );
	//if( res != 0 ) {
	//	return res;
	//}
	//res = playlist_add_file( manager->playlists[2], "http://kcrw.streamguys1.com/kcrw_192k_mp3_e24_internet_radio" );
	//if( res != 0 ) {
	//	return res;
	//}

	manager->version = 0;
	manager->current = 0;
	manager->len = 0;
	return 0;
}

int read_line(sds *buf, sds *line, int fd)
{
	int i = 0;
	char tmp[1024];
	for(;;) {
		if( (*buf)[i] == '\0' ) {
			// reached end of input, read more
			int i = read( fd, tmp, 1024 );
			if( i == 0 ) {
				*line = sdscpy( *line, *buf );
				return 1;
			}
			*buf = sdscatlen( *buf, tmp, i );
		}
		if( (*buf[i]) == '\n' ) {
			sdscpylen( *line, *buf, i - 1 );
			int j = i + 1;
			memmove( *buf, *buf + j, strlen(*buf + j) );
			sdsupdatelen( *buf );
			return 0;
		}
		i++;
	}
}

int playlist_manager_load( PlaylistManager *manager )
{
	int res;
	pthread_mutex_lock( &manager->lock );

	int fd = open( manager->playlistPath, O_RDONLY);
	if( fd < 0 ) {
		LOG_ERROR("err=d failed to open playlist", fd);
		pthread_mutex_unlock( &manager->lock );
		return 1;
	}

	sds buf = sdsempty();
	sds line = sdsempty();

	res = 0;
	while( !res ) {
		res = read_line( &buf, &line, fd );
		printf("got line %s", line );
	}

	sdsfree( buf );
	sdsfree( line );

	pthread_mutex_unlock( &manager->lock );
}

void playlist_manager_lock( PlaylistManager *manager )
{
	pthread_mutex_lock( &manager->lock );
}

void playlist_manager_unlock( PlaylistManager *manager )
{
	pthread_mutex_unlock( &manager->lock );
}

int playlist_manager_get_path( PlaylistManager *manager, int index, const char **path )
{
	if( 0 <= manager->current && manager->current < manager->len ) {
		Playlist *playlist = manager->playlists[manager->current];
		if( 0 <= index && index <= playlist->len ) {
			*path = playlist->list[index].path;
			return 0;
		}
	}
	return 1;
}

int playlist_manager_get_length( PlaylistManager *manager, int *len )
{
	if( 0 <= manager->current && manager->current < manager->len ) {
		Playlist *playlist = manager->playlists[manager->current];
		*len = playlist->len;
		return 0;
	}
	return 1;
}

int load_quick_album_recursive( PlaylistManager *manager, const char *path )
{
	int res;
	char p[1024];
	struct dirent *ent;

	LOG_DEBUG("path=s loading album", path);
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

int playlist_manager_set_playlist( PlaylistManager *manager, int id )
{
	int res = 1;

	res = pthread_mutex_lock( &manager->lock );
	if( res ) {
		return res;
	}

	if( 0 <= id && id < manager->len ) {
		manager->current = id;
		manager->version++;
		res = 0;
	}

	pthread_mutex_unlock( &manager->lock );
	return res;
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
	if( res ) {
		pthread_mutex_unlock( &manager->lock );
		return res;
	}

	LOG_DEBUG("sorting album");
	playlist_sort_by_path( manager->playlists[0] );
	playlist_rename( manager->playlists[0], basename(path) );
	manager->current = 0;
	manager->version++;
	pthread_mutex_unlock( &manager->lock );
	return OK;
}

int playlist_manager_next( PlaylistManager *manager )
{
	int res;
    res = pthread_mutex_lock( &manager->lock );
	if( res ) {
		return res;
	}

	manager->current++;
	if( manager->current >= manager->len ) {
		manager->current = 0;
	}

	manager->version++;
    pthread_mutex_unlock( &manager->lock );
	return res;
}

int playlist_manager_prev( PlaylistManager *manager )
{
	int res;
    res = pthread_mutex_lock( &manager->lock );
	if( res ) {
		return res;
	}

	manager->current--;
	if( manager->current < 0 ) {
		manager->current = manager->len - 1;
	}

	manager->version++;
    pthread_mutex_unlock( &manager->lock );
	return res;
}
