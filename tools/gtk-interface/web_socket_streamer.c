#include "web_socket_streamer.h"
#include "json_helpers.h"

#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <json-c/json.h>

PlaylistItemChanged global_callback;

typedef struct WSClientData {
	struct json_tokener *tok;
	//char *buf;
	//int i;
	//int capacity;
} WSClientData;

//void parse_json(const char *s, len int) {
//	json_object *root_obj;
//	root_obj = json_tokener_parse( s );
//
//	struct json_tokener *tok = json_tokener_new();
//	root_obj = json_tokener_parse_ex( tok, s, len );
//	json_tokener_free( tok );
//}


static struct lws *web_socket = NULL;

#define EXAMPLE_RX_BUFFER_BYTES (10)

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

static int callback_example( struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len )
{
	bool playing;
	int item_id;
	enum json_tokener_error jerr;
	json_object *root_obj;
	char *p;
	switch( reason )
	{
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
			lws_callback_on_writable( wsi );
			memset( user, 0, sizeof(WSClientData) );
			//((WSClientData*)user)->buf = malloc(1024);
			//((WSClientData*)user)->i = 0;
			//((WSClientData*)user)->capacity = 1024;
			((WSClientData*)user)->tok = json_tokener_new();
			break;

		case LWS_CALLBACK_CLIENT_RECEIVE:
			root_obj = json_tokener_parse_ex(
					((WSClientData*)user)->tok, (const char*)in, len);
			if( root_obj == NULL ) {
				jerr = json_tokener_get_error(((WSClientData*)user)->tok);
 				if( jerr == json_tokener_continue ) {
					//printf("json tokener continue\n");
					break;
				}
				printf("unable to parse json\n");
				return 1;
			}
			printf("got data\n");
			if( print_payload( root_obj, &playing, &item_id ) == 0 ) {
				global_callback( playing, item_id );
			} else {
				printf("failed to get track details\n");
			}
			if( (int) ((WSClientData*)user)->tok->char_offset < (int)len) {
				// TODO if this ever happens, I will need to buffer the left over chars
				// so they can be fed back into the parser to parse potentially two messages in a single packet
				printf("failed to read all data\n");
				return 1;
			}

			//p += ((WSClientData*)user)->i;
			//memcpy( p, in, len );
			//p[i+len+1] = '\0';
			//((WSClientData*)user)->i += len;

			//parse_json( ((WSClientData*)user)->buf, ((WSClientData*)user)->i );

			//printf("got %.*s\n",
			//		((WSClientData*)user)->i,
			//		((WSClientData*)user)->buf
			//		);

			//fflush(0);
			break;

		case LWS_CALLBACK_CLIENT_WRITEABLE:
		{
			unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + EXAMPLE_RX_BUFFER_BYTES + LWS_SEND_BUFFER_POST_PADDING];
			unsigned char *p = &buf[LWS_SEND_BUFFER_PRE_PADDING];
			size_t n = sprintf( (char *)p, "%u", rand() );
			lws_write( wsi, p, n, LWS_WRITE_TEXT );
			break;
		}

		case LWS_CALLBACK_CLOSED:
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			web_socket = NULL;
			//free( ((WSClientData*)user)->buf );
			break;

		default:
			break;
	}

	return 0;
}

enum protocols
{
	PROTOCOL_EXAMPLE = 0,
	PROTOCOL_COUNT
};

static struct lws_protocols protocols[] =
{
	{
		.name = "example-protocol",
		.callback = callback_example,
		.per_session_data_size = sizeof(WSClientData),
		.rx_buffer_size = EXAMPLE_RX_BUFFER_BYTES,
		.id = 0,
		.user = 0 /* not to be confused with per-connection user data */ 
	},
	{ NULL, NULL, 0, 0, 0, 0 } /* terminator */
};

void stream_websocket( const char *host, int port, PlaylistItemChanged callback )
{
	global_callback = callback;
	struct lws_context_creation_info info;
	memset( &info, 0, sizeof(info) );

	info.port = CONTEXT_PORT_NO_LISTEN;
	info.protocols = protocols;
	info.gid = -1;
	info.uid = -1;

	struct lws_context *context = lws_create_context( &info );

	time_t old = 0;
	while( 1 )
	{
		struct timeval tv;
		gettimeofday( &tv, NULL );

		/* Connect if we are not connected to the server. */
		if( !web_socket && tv.tv_sec != old )
		{
			struct lws_client_connect_info ccinfo = {0};
			ccinfo.context = context;
			ccinfo.address = host;
			ccinfo.port = port;
			ccinfo.path = "/websocket";
			ccinfo.host = lws_canonical_hostname( context );
			ccinfo.origin = "origin";
			ccinfo.protocol = protocols[PROTOCOL_EXAMPLE].name;
			web_socket = lws_client_connect_via_info(&ccinfo);
		}

		//if( tv.tv_sec != old )
		//{
		//	/* Send a random number to the server every second. */
		//	lws_callback_on_writable( web_socket );
		//	old = tv.tv_sec;
		//}

		lws_service( context, /* timeout_ms = */ 250 );
	}

	lws_context_destroy( context );
}




void* cb( void *user )
{
}

int start_stream_websocket_thread( WebsocketClient *data )
{
	if( pthread_create( &(data->thread), NULL, cb, data) ) {
		return 1;
	}
	if( pthread_mutex_init( &(data->lock), NULL ) != 0 ) {
		return 2;
	}
	return 0;
}

void websocket_client_connect( WebsocketClient *data, const char *hostname, int port )
{
}
