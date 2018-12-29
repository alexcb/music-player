#ifndef _WEB_SOCKET_STREAMER_H_
#define _WEB_SOCKET_STREAMER_H_

#include <stdbool.h>
#include <pthread.h>

typedef void (*PlaylistItemChanged)( bool playing, int item_id );

void stream_websocket( const char *host, int port, PlaylistItemChanged callback );

typedef struct {
	char hostname[1024];
	int port;
	PlaylistItemChanged callback;
	pthread_t thread;
	pthread_mutex_t lock;


} WebsocketClient;

int start_stream_websocket_thread( WebsocketClient *data );

void websocket_client_connect( WebsocketClient *data, const char *hostname, int port );

#endif // _WEB_SOCKET_STREAMER_H_
