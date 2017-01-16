#include "web.h"

#include "httpget.h"
#include "player.h"
#include "albums.h"
#include "playlist_manager.h"
#include "string_utils.h"
#include "log.h"
#include "errors.h"
#include "my_data.h"
#include "base64.h"

#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <microhttpd.h>
#include <json-c/json.h>
#include <openssl/sha.h>

#define MAX_CONNETIONS 64
#define MAX_PAYLOAD_SIZE 1024

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
	
} WebHandlerData;


// from mhd upgrade example
//static void make_blocking( MHD_socket fd )
//{
//	int flags;
//
//	flags = fcntl (fd, F_GETFL);
//	if (-1 == flags)
//		return;
//	if ((flags & ~O_NONBLOCK) != flags)
//		if (-1 == fcntl (fd, F_SETFL, flags & ~O_NONBLOCK))
//			abort ();
//}

int send_all( int sockfd, const char *buf, size_t len )
{
	int res;
	while( len > 0 ) {
		res = send( sockfd, buf, len, 0);
		if( errno == EAGAIN ) {
			res = 0;
			continue;
		}
		if( res < 0 ) {
			return SOCK_SEND_ERROR;
		}
		buf += res;
		len -= res;
	}
	return OK;
}

void clear_websocket_input( WebsocketData *ws )
{
	if( ws->extra_in ) {
		free( ws->extra_in );
		ws->extra_in = NULL;
	}
}

int websocket_send( WebsocketData *ws, const char *payload )
{
	return send_all( ws->sock, payload, strlen(payload));
}


int websocket_send_header( WebsocketData *ws, const char *header, const char *value )
{
	char buf[1024];
	snprintf(buf, 1024, "%s: %s\n", header, value);
	if( strlen(buf) > 1020 ) {
		return 1;
	}
	return send_all( ws->sock, buf, strlen(buf));
}


bool handle_websocket( WebsocketData *ws, const char *payload )
{
	int res;

	// discard this, we are not reading from this socket
	if( ws->extra_in ) {
		free( ws->extra_in );
		ws->extra_in = NULL;
	}

	res = send_all( ws->sock, payload, strlen(payload));
	if( res ) {
		goto error;
	}
	res = send_all( ws->sock, "\n", 1);
	if( res ) {
		goto error;
	}

	return true;

error:
	LOG_INFO( "disconnecting websocket" );
	free( ws );
	return false;
}

WebHandlerData *shitty_global = NULL;
void update_metadata_web_clients( bool playing, const char *playlist_name, const PlayerTrackInfo *track, void *data )
{
	LOG_DEBUG( "playlist=s artist=s title=s update_metadata_web_clients", playlist_name, track->artist, track->title );

	json_object *state = json_object_new_object();
	json_object_object_add( state, "artist", json_object_new_string( track->artist ) );
	json_object_object_add( state, "title", json_object_new_string( track->title ) );
	json_object_object_add( state, "playlist", json_object_new_string( playlist_name ) );

	const char *s = json_object_to_json_string( state );

	bool ok;
	if( shitty_global ) {
		pthread_mutex_lock( &shitty_global->connections_lock );
		for( int i = 0; i < shitty_global->num_connections; ) {
			ok = handle_websocket( shitty_global->connections[ i ], s );
			if( !ok ) {
				// remove websocket
				shitty_global->num_connections--;
				if( i < shitty_global->num_connections ) {
					shitty_global->connections[ i ] = shitty_global->connections[ shitty_global->num_connections ];
				}
			} else {
				i++;
			}
		}
		pthread_mutex_unlock( &shitty_global->connections_lock );
	}

	// this causes the json string to be released
	json_object_put( state );
}

int error_handler(struct MHD_Connection *connection, const char *fmt, ...)
{
	char buf[1024];
	va_list args;
	va_start( args, fmt );
	vsnprintf( buf, 1024, fmt, args );
	va_end( args );

	struct MHD_Response *response = MHD_create_response_from_buffer( strlen(buf), (void*) buf, MHD_RESPMEM_MUST_COPY );
	if( response == NULL ) {
		return MHD_NO;
	}
	int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
	MHD_destroy_response(response);
	return ret;
}

static ssize_t file_reader (void *cls, uint64_t pos, char *buf, size_t max)
{
	FILE *file = cls;

	(void)  fseek (file, pos, SEEK_SET);
	return fread (buf, 1, max, file);
}

static void free_callback (void *cls)
{
	FILE *file = cls;
	fclose (file);
}


static int web_handler_static(
		WebHandlerData *data,
		struct MHD_Connection *connection,
		const char *url,
		const char *method,
		const char *version,
		const char *upload_data,
		size_t *upload_data_size,
		void **con_cls)
{
	url = trim_prefix( url, "/static/" );

	//// TODO make this safe
	//int fd = open( url, O_RDONLY );
	//if( fd < 0 ) {
	//	return error_handler( connection, "unable to open file: %s", url );
	//}

	//struct stat stat;
	//int res = fstat( fd, &stat );
	//if( res ) {
	//	close( fd );
	//	return error_handler( connection, "unable to stat file: %s", url );
	//}

	//FILE *f = fopen(url, "r");
	//struct MHD_Response *response = MHD_create_response_from_fd( fd, stat.st_size );

	char path[1024];
	const char *root = getenv("WEB_ROOT");
	if( root ) {
		snprintf(path, 1024, "%s/%s", root, url);
	} else {
		snprintf(path, 1024, "%s", url);
	}

	FILE *file = fopen( path, "rb" );
	if( file == NULL ) {
		return error_handler( connection, "unable to open file: %s", url );
	}

	int fd = fileno( file );
	if( fd < 0 ) {
		fclose( file );
		return error_handler( connection, "unable to get file handle" );
	}

	struct stat stat;
	int res = fstat( fd, &stat );
	if( res ) {
		fclose( file );
		return error_handler( connection, "unable to stat file" );
	}

	struct MHD_Response *response = MHD_create_response_from_callback( stat.st_size, 32*1024, &file_reader, file, &free_callback );
	if (NULL == response) {
		fclose( file );
		return MHD_NO;
	}
	int ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
	// MHD_add_response_header( response, "Content-Type", MIMETYPE );
	MHD_destroy_response (response);
	return ret;
}

int register_websocket( WebHandlerData *data, WebsocketData *ws )
{
	int res = 0;
	pthread_mutex_lock( &data->connections_lock );

	if( data->num_connections < MAX_CONNETIONS ) {
		data->connections[data->num_connections] = ws;
		data->num_connections++;
	} else {
		res = 1;
	}

	pthread_mutex_unlock( &data->connections_lock );

	return res;
}

#define WEBSOCKET_ACCEPT_MAGIC "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

// out must be at least 30 bytes
void compute_key(const char *key, char *out)
{
	char buf[1024];
	snprintf(buf, 1024, "%s%s", key, WEBSOCKET_ACCEPT_MAGIC);

	LOG_DEBUG("key=s computing key", buf);
	unsigned char hash[SHA_DIGEST_LENGTH]; // == 20
	SHA1(buf, strlen(buf) - 1, hash);

	Base64encode( out, hash, SHA_DIGEST_LENGTH);
}

static void websocket_upgrade_handler(
		void *cls,
		struct MHD_Connection *connection,
		void *con_cls,
		const char *extra_in,
		size_t extra_in_size,
		MHD_socket sock,
		struct MHD_UpgradeResponseHandle *urh)
{
	int res;
	LOG_DEBUG("websocket_upgrade_handler called");
	WebHandlerData *data = (WebHandlerData*) cls;

	WebsocketData *ws = (WebsocketData*) malloc( sizeof(WebsocketData) );
	if( ws == NULL ) {
		abort();
	}
	if( extra_in_size > 0) {
		ws->extra_in = malloc( extra_in_size );
		if( ws->extra_in == NULL )
			abort ();
		memcpy( ws->extra_in, extra_in, extra_in_size );
	} else {
		ws->extra_in = NULL;
	}
	ws->extra_in_size = extra_in_size;
	ws->sock = sock;
	ws->urh = urh;

	// discard all cached input (we're not accepting input at the moment)
	clear_websocket_input( ws );

	res = register_websocket( data, ws );
	if( res ) {
		free( ws );
		abort();
	}

	// send a welcome message to websocket
	websocket_send( ws, "hello world" );
}


static int web_handler_websocket(
		WebHandlerData *data,
		struct MHD_Connection *connection,
		const char *url,
		const char *method,
		const char *version,
		const char *upload_data,
		size_t *upload_data_size,
		void **con_cls)
{
	const char *client_key = MHD_lookup_connection_value( connection, MHD_HEADER_KIND, "Sec-WebSocket-Key" );
	if( !client_key ) {
		LOG_WARN("websocket connection requestion without Sec-WebSocket-Key");
		return;
	}

	char accept_key[30];
	compute_key(client_key, accept_key);
	LOG_DEBUG("accept_key=s computed key", accept_key);

	struct MHD_Response *response = MHD_create_response_for_upgrade( &websocket_upgrade_handler, (void*) data );
	MHD_add_response_header( response, MHD_HTTP_HEADER_UPGRADE, "websocket" );
	MHD_add_response_header( response, "Sec-WebSocket-Accept", accept_key );


	int ret = MHD_queue_response( connection, MHD_HTTP_SWITCHING_PROTOCOLS, response );
	MHD_destroy_response( response );
	LOG_DEBUG("ret=d web_handler_websocket", ret);
	return ret;
}

static int web_handler_albums(
		WebHandlerData *data,
		struct MHD_Connection *connection,
		const char *url,
		const char *method,
		const char *version,
		const char *upload_data,
		size_t *upload_data_size,
		void **con_cls)
{
	LOG_DEBUG("in web_handler_albums");
	json_object *albums = json_object_new_array();
	for( int i = 0; i < data->album_list->len; i++ ) {
		json_object *album = json_object_new_object();
		json_object_object_add( album, "artist", json_object_new_string( data->album_list->list[i].artist ) );
		json_object_object_add( album, "name", json_object_new_string( data->album_list->list[i].name ) );
		json_object_array_add( albums, album );
	}

	json_object *root_obj = json_object_new_object();
	json_object_object_add( root_obj, "albums", albums );

	const char *s = json_object_to_json_string( root_obj );
	struct MHD_Response *response = MHD_create_response_from_buffer( strlen(s), (void*)s, MHD_RESPMEM_MUST_COPY );

	// this causes the json string to be released
	json_object_put( root_obj );

	int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
	MHD_destroy_response(response);
	return ret;
}

static int web_handler_albums_play(
		WebHandlerData *data,
		struct MHD_Connection *connection,
		const char *url,
		const char *method,
		const char *version,
		const char *upload_data,
		size_t *upload_data_size,
		void **con_cls)
{
	const char *id = MHD_lookup_connection_value( connection, MHD_GET_ARGUMENT_KIND, "album" );

	if( id != NULL ) {
		errno = 0;
		long int i = strtol(id, NULL, 10);
		bool ok = !errno;

		if( ok ) {
			if( 0 <= i && i < data->album_list->len ) {
				// TODO error handling
				load_quick_album( data->playlist_manager, data->album_list->list[i].path );
				// TODO need for a mutex here?
				data->player->restart = true;
			}
		}
	}

	struct MHD_Response *response = MHD_create_response_from_buffer( 2, "ok", MHD_RESPMEM_PERSISTENT );
	int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
	MHD_destroy_response(response);
	return ret;
}

static int web_handler(
		void *cls,
		struct MHD_Connection *connection,
		const char *url,
		const char *method,
		const char *version,
		const char *upload_data,
		size_t *upload_data_size,
		void **con_cls)
{
	WebHandlerData *data = (WebHandlerData*) cls;

	static int dummy;
	if (&dummy != *con_cls)
	{
		LOG_INFO("url=s method=s handling request", url, method);
		/* never respond on first call -- not sure why, but libhttpd does this everywhere */
		*con_cls = &dummy;
		return MHD_YES;
	}
	*con_cls = NULL; /* reset when done -- again, not sure what this does */

	if( strcmp( method, "GET" ) == 0 && strcmp(url, "/websocket") == 0 ) {
		return web_handler_websocket( data, connection, url, method, version, upload_data, upload_data_size, con_cls );
	}

	if( strcmp( method, "GET" ) == 0 && strcmp(url, "/albums") == 0 ) {
		return web_handler_albums( data, connection, url, method, version, upload_data, upload_data_size, con_cls );
	}

	if( strcmp( method, "POST" ) == 0 && strcmp(url, "/albums") == 0 ) {
		return web_handler_albums_play( data, connection, url, method, version, upload_data, upload_data_size, con_cls );
	}

	if( strcmp( method, "GET" ) == 0 && has_prefix(url, "/static/") ) {
		return web_handler_static( data, connection, url, method, version, upload_data, upload_data_size, con_cls );
	}

	if( strcmp( method, "GET" ) == 0 && strcmp(url, "/") == 0 ) {
		url = "/static/index.html";
		return web_handler_static( data, connection, url, method, version, upload_data, upload_data_size, con_cls );
	}

	return error_handler( connection, "unable to dispatch url: %s", url );
}

int start_http_server( AlbumList *album_list, PlaylistManager *playlist_manager, Player *player )
{
	WebHandlerData data = {
		.album_list = album_list,
		.playlist_manager = playlist_manager,
		.player = player,
		.num_connections = 0,
	};
	pthread_mutex_init( &data.connections_lock, NULL );

	// used to pass data to id3 tag callback
	shitty_global = &data;

	struct MHD_Daemon *d = MHD_start_daemon(
			MHD_USE_SELECT_INTERNALLY | MHD_USE_SUSPEND_RESUME,
			80,
			NULL,
			NULL,
			&web_handler,
			(void*) &data,
			MHD_OPTION_END);

	if( d == NULL ) {
		return 1;
	}


	LOG_DEBUG("busy looping");
	//for(;;) { sleep(10); printf("busy loop sleep\n"); }
	for(;;) {
		sleep(1);
	}
}
