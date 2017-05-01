#pragma once

#include "my_data.h"
#include <stdbool.h>
#include <microhttpd.h>

#define MAX_CONNETIONS 64
#define MAX_TRACK_PAYLOAD 4*1024

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
	char current_track_payload[MAX_TRACK_PAYLOAD];
	
} WebHandlerData;

void update_metadata_web_clients(bool playing, const PlayerTrackInfo *track, void *data);

int init_http_server_data( WebHandlerData *data, MyData *my_data );
int start_http_server( WebHandlerData *data );
