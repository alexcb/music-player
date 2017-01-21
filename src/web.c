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
#include <arpa/inet.h>


#include <json-c/json.h>
#include <openssl/sha.h>

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
	char header[16];
	int i = 0;
	size_t len = strlen(payload);
	unsigned char b1 = 0x01 | 0x80;
	unsigned char b2 = 0;
	header[i++] = b1;
	if( len < 126 ) {
		b2 = b2 | len;
		header[i++] = b2;
	} else if( len < 65536 ) {
		b2 = b2 | 126;
		header[i++] = b2;
		((uint16_t*) header)[i] = htons(len);
		i += 2;
	} else {
		b2 = b2 | 127;
		header[i++] = b2;
		((uint32_t*) header)[i] = htonl(len);
		i += 4;
	}

	int res = 0;
	res = res || send_all( ws->sock, header, i);
	res = res || send_all( ws->sock, payload, strlen(payload));
	return res;
}

void update_metadata_web_clients( bool playing, const char *playlist_name, const PlayerTrackInfo *track, void *d )
{
	WebHandlerData *data = (WebHandlerData*) d;
	LOG_DEBUG( "playlist=s artist=s title=s update_metadata_web_clients", playlist_name, track->artist, track->title );

	json_object *state = json_object_new_object();
	json_object_object_add( state, "playing", json_object_new_boolean( playing ) );
	json_object_object_add( state, "artist", json_object_new_string( track->artist ) );
	json_object_object_add( state, "title", json_object_new_string( track->title ) );
	json_object_object_add( state, "playlist", json_object_new_string( playlist_name ) );

	const char *s = json_object_to_json_string( state );

	pthread_mutex_lock( &data->current_track_payload_lock );
	strcpy( data->current_track_payload, s );
	pthread_mutex_unlock( &data->current_track_payload_lock );
	pthread_cond_signal( &data->current_track_payload_cond );

	// this causes the json string to be released
	json_object_put( state );
}

void websocket_push_thread( WebHandlerData *data )
{
	int res;
	int cond_wait_res;
	char local_payload[MAX_TRACK_PAYLOAD];
	for(;;) {
		pthread_mutex_lock( &data->current_track_payload_lock );
		cond_wait_res = pthread_cond_wait( &data->current_track_payload_cond, &data->current_track_payload_lock );
		strcpy( local_payload, data->current_track_payload );
		pthread_mutex_unlock( &data->current_track_payload_lock );
		if( cond_wait_res ) {
			LOG_DEBUG( "err=d pthread_cond_wait failed", cond_wait_res );
			continue;
		}

		pthread_mutex_lock( &data->connections_lock );
		LOG_DEBUG( "num_connections=d state=s updating web clients  ", data->num_connections, local_payload );
		for( int i = 0; i < data->num_connections; ) {
			res = websocket_send( data->connections[ i ], local_payload );
			if( res ) {
				LOG_ERROR( "removing websocket due to send error" );
				// remove websocket
				data->num_connections--;
				if( i < data->num_connections ) {
					data->connections[ i ] = data->connections[ data->num_connections ];
				}
			} else {
				i++;
			}
		}
		pthread_mutex_unlock( &data->connections_lock );
	}
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

		websocket_send( ws, data->current_track_payload );
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

	SHA1((const unsigned char *)buf, strlen(buf), hash);
	Base64encode( out, (const char*)hash, SHA_DIGEST_LENGTH);
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
		return MHD_NO; //todo figure out if this is the right way to cleanup
	}

	char accept_key[30];
	compute_key(client_key, accept_key);

	struct MHD_Response *response = MHD_create_response_for_upgrade( &websocket_upgrade_handler, (void*) data );
	MHD_add_response_header( response, MHD_HTTP_HEADER_UPGRADE, "websocket" );
	MHD_add_response_header( response, "Sec-WebSocket-Accept", accept_key );


	int ret = MHD_queue_response( connection, MHD_HTTP_SWITCHING_PROTOCOLS, response );
	MHD_destroy_response( response );
	LOG_DEBUG("ret=d web_handler_websocket", ret);
	return ret;
}

static int web_handler_playlists(
		WebHandlerData *data,
		struct MHD_Connection *connection,
		const char *url,
		const char *method,
		const char *version,
		const char *upload_data,
		size_t *upload_data_size,
		void **con_cls)
{
	LOG_DEBUG("in web_handler_playlists");

	json_object *playlists = json_object_new_array();

	playlist_manager_lock( data->playlist_manager );

	Playlist *p;
	for( int i = 0; i < data->playlist_manager->len; i++ ) {
		json_object *playlist = json_object_new_object();
		json_object_object_add( playlist, "name", json_object_new_string( p->name ) );

		json_object *items = json_object_new_array();
		p = data->playlist_manager->playlists[i];
		for( int j = 0; j < p->len; j++ ) {
			json_object *item = json_object_new_object();
			json_object_object_add( item, "path", json_object_new_string( p->list[j].path ) );
			json_object_array_add( items, item );
		}
		json_object_object_add( playlist, "items", items );

		json_object_array_add( playlists, playlist );
	}

	playlist_manager_unlock( data->playlist_manager );
	

	const char *s = json_object_to_json_string( playlists );
	struct MHD_Response *response = MHD_create_response_from_buffer( strlen(s), (void*)s, MHD_RESPMEM_MUST_COPY );

	// this causes the json string to be released
	json_object_put( playlists );

	int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
	MHD_destroy_response(response);
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

	if( strcmp( method, "GET" ) == 0 && strcmp(url, "/playlists") == 0 ) {
		return web_handler_playlists( data, connection, url, method, version, upload_data, upload_data_size, con_cls );
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

int init_http_server_data( WebHandlerData *data, AlbumList *album_list, PlaylistManager *playlist_manager, Player *player )
{
	data->album_list = album_list;
	data->playlist_manager = playlist_manager,
	data->player = player,
	data->num_connections = 0,

	pthread_mutex_init( &data->connections_lock, NULL );
	pthread_mutex_init( &data->current_track_payload_lock, NULL );

	pthread_cond_init( &data->current_track_payload_cond, NULL );

	data->current_track_payload[0] = '\0';

	return 0;
}

int start_http_server( WebHandlerData *data )
{
	struct MHD_Daemon *d = MHD_start_daemon(
			MHD_USE_SELECT_INTERNALLY | MHD_USE_SUSPEND_RESUME,
			80,
			NULL,
			NULL,
			&web_handler,
			(void*) data,
			MHD_OPTION_END);

	if( d == NULL ) {
		return 1;
	}


	LOG_DEBUG("running websocket push thread");
	websocket_push_thread( data );
	return 0;
}
