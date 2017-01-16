#pragma once

#include "my_data.h"
#include <stdbool.h>
#include <microhttpd.h>

#define MAX_CONNETIONS 64

typedef struct WebsocketData {
	struct MHD_UpgradeResponseHandle *urh;
	char *extra_in;
	size_t extra_in_size;
	MHD_socket sock;
} WebsocketData;


typedef struct WebHandlerData {
	AlbumList *album_list;
	PlaylistManager *playlist_manager;
	Player *player;

	//websocket related entries
	WebsocketData *connections[MAX_CONNETIONS];
	int num_connections;
	pthread_mutex_t connections_lock;
	char current_track_payload[1024]; 
	
} WebHandlerData;

void update_metadata_web_clients(bool playing, const char *playlist_name, const PlayerTrackInfo *track, void *data);

int init_http_server_data( WebHandlerData *data, AlbumList *album_list, PlaylistManager *playlist_manager, Player *player );
int start_http_server( WebHandlerData *data );
