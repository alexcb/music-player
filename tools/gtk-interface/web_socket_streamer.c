#include "web_socket_streamer.h"
#include "json_helpers.h"
#include "line_reader.h"

#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <json-c/json.h>
#include <unistd.h>
#include <assert.h>

PlaylistItemChanged global_callback;

int print_payload( json_object *obj, bool *playing, int *item_id )
{
	json_object *js;
	if( !get_json_object( obj, "type", json_type_string, &js ) ) {
		return 1;
	}
	const char *s = json_object_get_string( js );
	printf("type: %s\n", s);
	if( strcmp( s, "status" ) != 0 ) {
		return 1;
	}

	if( !get_json_object_bool( obj, "playing", playing ) ) {
		return 1;
	}

	if( !get_json_object_int( obj, "id", item_id ) ) {
		return 1;
	}
	return 0;
}

int connect_server( WebsocketClient *data )
{
	if( data->socket >= 0 ) {
		close( data->socket );
	}
	// create socket
	data->socket = socket(AF_INET, SOCK_STREAM, 0);
	if( data->socket < 0 ) {
		perror("socket");
		printf("socket fail\n");
		return 1; // TODO
	}

	struct hostent *server;
	printf("lookup %s\n", data->hostname);
	server = gethostbyname( data->hostname );
	if (server == NULL) {
		perror("gethostbyname");
		fprintf(stderr,"ERROR, no such host as %s\n", data->hostname);
		return 1;
	}

	struct sockaddr_in serveraddr;
	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);
	serveraddr.sin_port = htons(data->port);

	printf("using port %d\n", data->port);
	if( connect( data->socket, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) != 0 ) {
		perror("connect");
		return 1;
	}

	printf( "connected to %s %d\n", data->hostname, data->port );
	return 0;
}

#define WAITING_STATE 0
#define RESTART_STATE 1
#define UPGRADE_STATE 2
#define MESSAGE_FRAME_A_STATE 3
#define MESSAGE_FRAME_B_STATE 4
#define MESSAGE_STATE 5

void* cb( void *user )
{
	int res;
	bool restart = true;
	bool quit = false;
	WebsocketClient *data = (WebsocketClient*) user;
	bool looking_for_upgrade = true;
	int num_new_lines = 0;
	char buf[1024];

	struct json_tokener *tok = json_tokener_new();
	enum json_tokener_error jerr;
	json_object *root_obj;
	bool playing;
	int item_id;

	size_t n;

	BufferedStreamer buffered_stream;
	init_buffered_stream( &buffered_stream, 0, 1024 );

	int state = WAITING_STATE;
	int message_size = 0;
	int message_header_size = 0;

	for(;;) {
		pthread_mutex_lock( &(data->lock) );
		if( data->reconnect ) {
			data->reconnect = false;
			state = RESTART_STATE;
		}
		if( data->quit ) {
			quit = true;
		}
		pthread_mutex_unlock( &(data->lock) );
		if( quit ) {
			break;
		}
		switch( state ) {
			case WAITING_STATE:
				usleep(1000);
				break;
			case RESTART_STATE:
				printf("dealing with restart\n");
				restart = false;
				looking_for_upgrade = true;
				res = connect_server( data );
				if( res != 0 ) {
					printf("unhandled failure\n");
					goto wait_for_more;
				}
				reset_buffered_stream( &buffered_stream, data->socket );
				dprintf( data->socket, "GET /websocket HTTP/1.1\n" );
				dprintf( data->socket, "Upgrade: websocket\n" );
				dprintf( data->socket, "Connection: Upgrade\n" );
				dprintf( data->socket, "Host: %s:%d\n", data->hostname, data->port );
				dprintf( data->socket, "Origin: http://%s:%d\n", data->hostname, data->port );
				dprintf( data->socket, "Sec-WebSocket-Key: jIrvKwHGnbfJ3jliWiIPQw==\n" );
				dprintf( data->socket, "Sec-WebSocket-Version: 13\n" );
				dprintf( data->socket, "\n" );

				state = UPGRADE_STATE;
				// fall through

			case UPGRADE_STATE:
				for(;;) {
					res = read_line( &buffered_stream, buf, &n );
					if( res == BUFFERED_STREAM_EAGAIN ) {
						goto wait_for_more;
					} else if( res != BUFFERED_STREAM_OK ) {
						assert( 0 );
					}
					printf("got line (%ld): %.*s\n", n, (int) n, buf );
					fflush(0);
					if( buf[0] == '\0' ) {
						break;
					}
				}
				printf("connection upgraded\n");
				state = MESSAGE_FRAME_A_STATE;
				// fall through

			// should be 8112 7b22

			case MESSAGE_FRAME_A_STATE:
				res = read_bytes( &buffered_stream, 2, buf );
				if( res == BUFFERED_STREAM_EAGAIN ) {
					goto wait_for_more;
				} else if( res != BUFFERED_STREAM_OK ) {
					assert( 0 );
				}
				printf("got %x\n", ((uint8_t*)buf)[0]);
				printf("got %x\n", ((uint8_t*)buf)[1]);
				assert( (uint8_t)buf[0] == 0x81 );
				switch( buf[1] ) {
					case 127:
						message_header_size = 4;
						break;
					case 126:
						message_header_size = 2;
						break;
					default:
						message_header_size = 0;
						message_size = (uint8_t) buf[1];
						break;
				}
				state = MESSAGE_FRAME_B_STATE;
				// fall through

			case MESSAGE_FRAME_B_STATE:

				printf("expecting %d sized header message\n", message_header_size);
				res = read_bytes( &buffered_stream, message_header_size, buf );
				if( res == BUFFERED_STREAM_EAGAIN ) {
					goto wait_for_more;
				} else if( res != BUFFERED_STREAM_OK ) {
					assert( 0 );
				}

				switch( message_header_size ) {
					case 0:
						break;
					case 2:
						message_size = ntohs( *((uint16_t*)buf) );
						break;
					case 4:
						message_size = ntohs( *((uint32_t*)buf) );
						break;
					default:
						break;
				}
				state = MESSAGE_STATE;
				// fall through

			case MESSAGE_STATE:
				printf("expecting %d sized message\n", message_size);
				res = read_bytes( &buffered_stream, message_size, buf );
				if( res == BUFFERED_STREAM_EAGAIN ) {
					goto wait_for_more;
				} else if( res != BUFFERED_STREAM_OK ) {
					assert( 0 );
				}
				printf("handle %.*s \n", (int)message_size, buf);
				state = MESSAGE_FRAME_A_STATE;
				break;

			default:
				break;
		}
wait_for_more:
		usleep(100); // TODO use select
	}
	printf("exiting\n");
	return NULL;
}

int start_stream_websocket_thread( WebsocketClient *data )
{
	memset(data, 0, sizeof(WebsocketClient));
	data->socket = -1;
	pipe( data->wakeup_fd ); // read, write
	if( pthread_mutex_init( &(data->lock), NULL ) != 0 ) {
		return 1;
	}
	if( pthread_create( &(data->thread), NULL, cb, data) ) {
		return 2;
	}
	return 0;
}

void websocket_client_connect( WebsocketClient *data, const char *hostname, int port )
{
	pthread_mutex_lock( &(data->lock) );
	data->reconnect = true;
	strcpy( data->hostname, hostname );
	data->port = port;
	pthread_mutex_unlock( &(data->lock) );
}

int split_host_port(const char *s, char *hostname, int *port)
{
	char *p = strstr(s, ":");
	if( p == NULL ) {
		strcpy( hostname, s );
		*port = 80;
		return 0;
	}
	int i = p - s;
	memcpy( hostname, s, i );
	hostname[i] = '\0';
	*port = atoi(&s[i+1]);
	return 0;
}

