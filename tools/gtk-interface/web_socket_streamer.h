#ifndef _WEB_SOCKET_STREAMER_H_
#define _WEB_SOCKET_STREAMER_H_

#include <stdbool.h>
#include <pthread.h>

typedef void (*PlaylistItemChanged)( bool playing, int item_id );

typedef struct {
	char hostname[1024];
	int port;
	PlaylistItemChanged callback;
	pthread_t thread;
	pthread_mutex_t lock;
	//pthread_cond_t cond;
	int  wakeup_fd[2];
	bool reconnect;
	bool quit;
	int socket;
} WebsocketClient;

int start_stream_websocket_thread( WebsocketClient *data, PlaylistItemChanged callback );

void websocket_client_connect( WebsocketClient *data, const char *hostname, int port );

void websocket_client_join( WebsocketClient *data );

int split_host_port(const char *s, char *hostname, int *port);

#endif // _WEB_SOCKET_STREAMER_H_
