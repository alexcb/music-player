#include <ao/ao.h>
#include <mpg123.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>

#include <dirent.h>


#include "httpget.h"
#include "player.h"
#include "albums.h"
#include "playlist_manager.h"
#include "playlist.h"
#include "string_utils.h"
#include "log.h"
#include "errors.h"
#include "my_data.h"
#include "web.h"
#include "streams.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "id3.h"

struct httpdata hd;

void change_playlist( Player *player, PlaylistManager *manager, int dir )
{
	LOG_DEBUG("player=p manager=p change_playlist", player, manager);
	PlaylistItem *x = player->current_track.playlist_item;
	if( !x ) {
		LOG_ERROR("no playlist item");
		return;
	}
	Playlist *p = x->parent;
	if( !p ) {
		LOG_ERROR("no parent playlist");
		return;
	}
	if( dir > 0 ) {
		p = p->next;
		if( p == NULL ) {
			p = manager->root;
		}
	} else if( dir < 0 ) {
		p = p->prev;
		if( p == NULL ) {
			p = manager->root;
			if( p ) {
				while( p->next ) {
					p = p->next;
				}
			}
		}
	}
	if( p ) {
		PlaylistItem *x = p->root;
		if( p->current && p->current->next ) {
			x = p->current->next;
		}
		player_change_track( player, x, TRACK_CHANGE_IMMEDIATE );
	}
}


int main(int argc, char *argv[])
{
	int res;
	Player player;
	ID3Cache *cache;

	if( argc != 4 ) {
		printf("%s <albums path> <streams path> <playlist path>\n", argv[0]);
		return 1;
	}
	char *music_path = argv[1];
	char *streams_path = argv[2];
	char *playlist_path = argv[3];

	init_player( &player );

	res = id3_cache_new( &cache, "/tmp/id3_cache", player.mh );
	if( res ) {
		LOG_ERROR("failed to load cache");
		return 1;
	}

	StreamList stream_list;
	stream_list.p = NULL;
	res = parse_streams( streams_path, &stream_list );
	if( res ) {
		LOG_CRITICAL("failed to load streams");
		return 1;
	}

	AlbumList album_list;
	res = album_list_init( &album_list, cache );
	if( res ) {
		LOG_CRITICAL("err=d failed to init album list", res);
		return 1;
	}
	res = album_list_load( &album_list, music_path );
	if( res ) {
		LOG_CRITICAL("err=d failed to load albums", res);
		return 1;
	}

	res = id3_cache_save( cache );
	if( res ) {
		LOG_ERROR("err=d failed to save id3 cache", res);
	}
	
	PlaylistManager playlist_manager;
	playlist_manager_init( &playlist_manager, playlist_path );
	player.playlist_manager = &playlist_manager;

	LOG_DEBUG("calling load");
	res = playlist_manager_load( &playlist_manager );
	if( res ) {
		LOG_WARN("failed to load playlist");
	}
	LOG_DEBUG("load done");

	LOG_DEBUG("starting");

	res = start_player( &player );
	if( res ) {
		LOG_CRITICAL("failed to start player");
		return 1;
	}

	MyData my_data = {
		&player,
		&album_list,
		&playlist_manager,
		&stream_list
	};

	WebHandlerData web_handler_data;

	res = init_http_server_data( &web_handler_data, &my_data );
	if( res ) {
		LOG_ERROR("failed to init http server");
		return 2;
	}

	res = player_add_metadata_observer( &player, &update_metadata_web_clients, (void*) &web_handler_data );
	if( res ) {
		LOG_ERROR("failed to register observer");
		return 1;
	}

	if( playlist_manager.root ) {
		PlaylistItem *x = playlist_manager.root->root;
		player_change_track( &player, x, TRACK_CHANGE_IMMEDIATE );
	}

	LOG_DEBUG("running server");
	res = start_http_server( &web_handler_data );
	if( res ) {
		LOG_CRITICAL("failed to start http server");
		return 2;
	}

	LOG_DEBUG("done");
	return 0;
}
