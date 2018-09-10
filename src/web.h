#pragma once

#include "my_data.h"
#include "sds.h"
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
	MyData *my_data;

	//websocket related entries
	WebsocketData *connections[MAX_CONNETIONS];
	int num_connections;
	pthread_mutex_t connections_lock;

	pthread_cond_t current_track_payload_cond;
	pthread_mutex_t current_track_payload_lock;
	sds current_track_payload;
	
} WebHandlerData;

void update_metadata_web_clients(bool playing, const PlaylistItem *item, int playlist_version, void *data);

int get_library_json( StreamList *streams, AlbumList *album_list, sds *output );

int init_http_server_data( WebHandlerData *data, MyData *my_data );
int start_http_server( WebHandlerData *data );
