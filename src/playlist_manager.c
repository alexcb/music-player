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
#include <stdlib.h>



int playlist_manager_init( PlaylistManager *manager, const char *path, Library *library )
{
	//int res;

    if( pthread_mutex_init( &manager->lock, NULL ) != 0 ) {
		return 1;
	}

	manager->playlistPath = sdsnew( path );
	manager->root = NULL;
	manager->library = library;
	return 0;
}

int playlist_manager_save( PlaylistManager *manager )
{
	FILE *fp = fopen ( manager->playlistPath, "w" );
	if( !fp ) {
		LOG_ERROR("path=s failed to open playlist for writing", manager->playlistPath);
		return 1;
	}
	for( Playlist *x = manager->root; x != NULL && x->next != manager->root; x = x->next ) {
		fprintf( fp, "%s\n", x->name );
		for( PlaylistItem *i = x->root; i != NULL; i = i->next ) {
			if( i->track ) {
				fprintf( fp, " %s\n", i->track->path );
			} else if( i->stream ) {
				fprintf( fp, " %s\n", i->stream );
			} else {
				assert(0);
			}
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
	int res;
	Track *track;
	Playlist *playlist = NULL;
	PlaylistItem *root = NULL;
	PlaylistItem *item = NULL;
	PlaylistItem *last = NULL;

	FILE *fp = fopen ( manager->playlistPath, "r" );
	if( !fp ) {
		LOG_ERROR("path=s failed to open playlist for reading", manager->playlistPath);
		return 1;
	}

	char *stream = NULL;
	char *line = NULL;
	size_t len = 0;
	while( getline( &line, &len, fp ) != -1 ) {
		trim_suffix_newline( line );
		LOG_DEBUG("line=s got line", line);
		if( !line[0] ) {
			continue;
		}
		if( line[0] == ' ' && playlist ) {
			if( has_prefix( &line[1], "http://" ) ) {
				track = NULL;
				stream = (char*) sdsnew( &line[1] );
			} else {
				stream = NULL;
				res = library_get_track( manager->library, &line[1], &track );
				if( res != 0 ) {
					LOG_ERROR("res=d path=s failed to lookup track", res, &line[1]);
					continue;
				}
			}

			item = (PlaylistItem*) malloc( sizeof(PlaylistItem) );
			item->track = track;
			item->stream = stream;
			item->id = 0;
			item->ref_count = 1;

			item->next = NULL;
			item->parent = NULL;

			if( root == NULL ) {
				root = item;
			} else {
				last->next = item;
			}
			last = item;
		} else if( line[0] != ' ' ) {
			// new playlist
			if( root != NULL ) {
				playlist_update( playlist, root );
				root = NULL;
			}
			playlist_manager_new_playlist( manager, line, &playlist );
		} else {
			LOG_ERROR("line=s skipping line while loading", line);
		}
	}
	if( root != NULL ) {
		playlist_update( playlist, root );
		root = NULL;
	}
	fclose( fp );
	return 0;
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
	for( Playlist *x = manager->root; x != NULL && x->next != manager->root; x = x->next ) {
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
	if( manager->root == NULL ) {
		manager->root = *p;
		(*p)->next = (*p)->prev = *p;
	} else {
		Playlist *tmp = manager->root->next;
		manager->root->next = *p;
		(*p)->next = tmp;
		tmp->prev = *p;
	}
	return 0;
}

int playlist_manager_get_item_by_id( PlaylistManager *manager, int id, PlaylistItem **i )
{
	for( Playlist *x = manager->root; x != NULL && x->next != manager->root; x = x->next ) {
		for( PlaylistItem *j = x->root; j != NULL; j = j->next ) {
			if( j->id == id ) {
				*i = j;
				return OK;
			}
		}
	}
	return NOT_FOUND;
}

int playlist_manager_checksum( PlaylistManager *manager )
{
	int checksum = 0;
	for( Playlist *x = manager->root; x != NULL && x->next != manager->root; x = x->next ) {
		checksum += x->id;
	}
	return checksum;
}
