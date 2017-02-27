#define _GNU_SOURCE // to get the gnu version of basename
#include "playlist_manager.h"

#include "string_utils.h"
#include "errors.h"
#include "log.h"
#include "sglib.h"

#include <dirent.h> 
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>



int playlist_manager_init( PlaylistManager *manager, const char *path )
{
	//int res;

    if( pthread_mutex_init( &manager->lock, NULL ) != 0 ) {
		return 1;
	}

	manager->playlistPath = sdsnew( path );
	manager->root = NULL;
	return 0;
}

int playlist_manager_save( PlaylistManager *manager )
{
	FILE *fp = fopen ( manager->playlistPath, "w" );
	if( !fp ) {
		LOG_ERROR("path=s failed to open playlist for writing", manager->playlistPath);
		return 1;
	}
	for( Playlist *x = manager->root; x != NULL; x = x->next ) {
		fprintf( fp, "%s\n", x->name );
		for( PlaylistItem *i = x->root; i != NULL; i = i->next ) {
			fprintf( fp, " %s\n", i->path );
		}
	}
	fclose( fp );
	return 0;
}

void trim_suffix_newline(char *s)
{
	int i = strlen(s) - 1;
	while( i >= 0 && s[i] == '\n' ) {
		s[i] = '\0';
		i--;
	}
}

int playlist_manager_load( PlaylistManager *manager )
{
	FILE *fp = fopen ( manager->playlistPath, "r" );
	if( !fp ) {
		LOG_ERROR("path=s failed to open playlist for reading", manager->playlistPath);
		return 1;
	}

	Playlist *playlist = NULL;
	char *line = NULL;
	size_t len = 0;
	while( getline( &line, &len, fp ) != -1 ) {
		trim_suffix_newline( line );
		LOG_DEBUG("line=s got line", line);
		if( !line[0] )
			continue;
		if( line[0] == ' ' && playlist ) {
			playlist_add_file( playlist, &line[1] );
		}
		else if( line[0] != ' ' ) {
			playlist_manager_new_playlist( manager, line, &playlist );
		}
	}
	fclose( fp );
	return 0;
}

void playlist_manager_lock( PlaylistManager *manager )
{
	pthread_mutex_lock( &manager->lock );
}

void playlist_manager_unlock( PlaylistManager *manager )
{
	pthread_mutex_unlock( &manager->lock );
}

int playlist_manager_delete_playlist( PlaylistManager *manager, const char *name )
{
	Playlist *p = manager->root;
	while( p != NULL ) {
		if( strcmp(p->name, name) == 0 ) {
			if( p->prev ) {
				p->prev->next = p->next;
			} else {
				manager->root = p->next;
			}
			if( p->next ) {
				p->next->prev = p->prev;
			}
			playlist_ref_down( p );
			break;
		}
		p = p->next;
	}
	return 0;
}

int playlist_manager_get_playlist( PlaylistManager *manager, const char *name, Playlist **p )
{
	for( Playlist *x = manager->root; x != NULL; x = x->next ) {
		if( strcmp(x->name, name) == 0 ) {
			*p = x;
			return 0;
		}
	}
	return 1;
}

int playlist_manager_new_playlist( PlaylistManager *manager, const char *name, Playlist **p )
{
	int res;
	res = playlist_manager_get_playlist( manager, name, p );
	if( res == 0 ) {
		return 1;
	}

	playlist_new( p, name );
	(*p)->prev = NULL;
	(*p)->next = manager->root;
	if( manager->root ) {
		manager->root->prev = *p;
	}
	manager->root = *p;
	return 0;
}

int load_quick_album_recursive( PlaylistManager *manager, const char *path )
{
	//int res;
	//char p[1024];
	//struct dirent *ent;

	//LOG_DEBUG("path=s loading album", path);
	//DIR *d = opendir( path );
	//if( d == NULL ) {
	//	return 1;
	//}

	//while( (ent = readdir(d)) != NULL) {
	//	sprintf(p, "%s/%s", path, ent->d_name);
	//	if( ent->d_type == DT_REG && has_suffix(p, ".mp3") ) {
	//		res = playlist_add_file( manager->playlists[0], p );
	//		if( res ) {
	//			closedir( d );
	//			return res;
	//		}
	//	} else if( ent->d_type == DT_DIR && strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0 ) {
	//		res = load_quick_album_recursive( manager, p );
	//		if( res ) {
	//			closedir( d );
	//			return res;
	//		}
	//	}
	//}
	//closedir( d );
	return 0;
}

int load_quick_album( PlaylistManager *manager, const char *path )
{
	//int res;
	//res = pthread_mutex_lock( &manager->lock );
	//if( res ) {
	//	return res;
	//}

	//res = playlist_clear( manager->playlists[0] );
	//if( res ) {
	//	pthread_mutex_unlock( &manager->lock );
	//	return res;
	//}

	//res = load_quick_album_recursive( manager, path );
	//if( res ) {
	//	pthread_mutex_unlock( &manager->lock );
	//	return res;
	//}

	//LOG_DEBUG("sorting album");
	//playlist_sort_by_path( manager->playlists[0] );
	//playlist_rename( manager->playlists[0], basename(path) );
	//manager->current = 0;
	//manager->version++;
	//pthread_mutex_unlock( &manager->lock );
	return OK;
}

int playlist_manager_get_item_by_id( PlaylistManager *manager, int id, PlaylistItem **i )
{
	for( Playlist *x = manager->root; x != NULL; x = x->next ) {
		for( PlaylistItem *j = x->root; j != NULL; j = j->next ) {
			if( j->id == id ) {
				*i = j;
				return OK;
			}
		}
	}
	return NOT_FOUND;
}
