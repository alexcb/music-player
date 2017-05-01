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

typedef struct RequestSession {
	sds body;
} RequestSession;

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

void update_metadata_web_clients( bool playing, const PlayerTrackInfo *track, void *d )
{
	char pointer[16];
	WebHandlerData *data = (WebHandlerData*) d;
	LOG_DEBUG( "playlistitem=p update_metadata_web_clients", track->playlist_item );

	json_object *state = json_object_new_object();
	json_object_object_add( state, "playing", json_object_new_boolean( playing ) );
	if( track->playlist_item ) {
		json_object_object_add( state, "artist", json_object_new_string( track->playlist_item->title ) );
		json_object_object_add( state, "title", json_object_new_string( track->playlist_item->title ) );
	}
	sprintf( pointer, "%p", track->playlist_item );
	json_object_object_add( state, "id", json_object_new_string( pointer ) );

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
		LOG_DEBUG( "num_connections=d state=s updating web clients", data->num_connections, local_payload );
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
		const sds request_body,
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
		const sds request_body,
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

static int web_handler_playlists_delete(
		WebHandlerData *data,
		struct MHD_Connection *connection,
		const char *url,
		const char *method,
		const char *version,
		const sds request_body,
		void **con_cls)
{
	const char *name = MHD_lookup_connection_value( connection, MHD_GET_ARGUMENT_KIND, "name" );
	LOG_DEBUG("in web_handler_playlists_delete");

	if( name != NULL && *name ) {
		playlist_manager_delete_playlist( data->my_data->playlist_manager, name );
	}

	struct MHD_Response *response = MHD_create_response_from_buffer( 2, "ok", MHD_RESPMEM_PERSISTENT );
	int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
	MHD_destroy_response(response);
	return ret;
}

static int web_handler_playlists_load(
		WebHandlerData *data,
		struct MHD_Connection *connection,
		const char *url,
		const char *method,
		const char *version,
		const sds request_body,
		void **con_cls)
{
	const char *name;
	const char *s;
	json_object *root_obj, *playlist_obj, *element_obj, *name_obj;
	Playlist *playlist;

	root_obj = json_tokener_parse( request_body );
	if( !json_object_is_type( root_obj, json_type_object ) ) {
		return error_handler( connection, "failed decoding json" );
	}

	if( !json_object_object_get_ex( root_obj, "name", &name_obj ) ) {
		return error_handler( connection, "failed decoding json" );
	}
	name = json_object_get_string( name_obj );
	if( !name ) {
		return error_handler( connection, "failed decoding json" );
	}

	if( !json_object_object_get_ex( root_obj, "playlist", &playlist_obj ) ) {
		return error_handler( connection, "failed decoding json" );
	}

	// create new playlist (or get pre-existing)
	playlist_manager_lock( data->my_data->playlist_manager );
	playlist_manager_new_playlist( data->my_data->playlist_manager, name, &playlist );
	playlist_clear( playlist );

	int len = json_object_array_length( playlist_obj );
	for( int i = 0; i < len; i++ ) {
		element_obj = json_object_array_get_idx( playlist_obj, i );
		if( !json_object_is_type( element_obj, json_type_string ) ) {
			return error_handler( connection, "expected json array with string elements" );
		}
		s = json_object_get_string( element_obj );
		playlist_add_file( playlist, s );
	}
	playlist_manager_save( data->my_data->playlist_manager );
	playlist_manager_unlock( data->my_data->playlist_manager );

	struct MHD_Response *response = MHD_create_response_from_buffer( 2, "ok", MHD_RESPMEM_PERSISTENT );
	int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
	MHD_destroy_response(response);
	return ret;
}

static int web_handler_playlists_play(
		WebHandlerData *data,
		struct MHD_Connection *connection,
		const char *url,
		const char *method,
		const char *version,
		const sds request_body,
		void **con_cls)
{
	Playlist *playlist;
	int res;
	int track = 0;
	const char *name = MHD_lookup_connection_value( connection, MHD_GET_ARGUMENT_KIND, "name" );
	const char *trackStr = MHD_lookup_connection_value( connection, MHD_GET_ARGUMENT_KIND, "track" );
	if( trackStr ) {
		track = atoi( trackStr );
	}
	if( !name ) {
		return error_handler( connection, "no name given" );
	}

	res = playlist_manager_get_playlist( data->my_data->playlist_manager, name, &playlist );
	if( res ) {
		return error_handler( connection, "failed to find playlist" );
	}

	PlaylistItem *x;
	for( x = playlist->root; x && track > 0; x = x->next, track-- );
	if( track > 0 ) {
		return error_handler( connection, "track greater than playlist length" );
	}
	player_change_track( data->my_data->player, x, TRACK_CHANGE_IMMEDIATE );

	return error_handler( connection, "ok" );
}

static int web_handler_playlists(
		WebHandlerData *data,
		struct MHD_Connection *connection,
		const char *url,
		const char *method,
		const char *version,
		const sds request_body,
		void **con_cls)
{
	LOG_DEBUG("in web_handler_playlists");

	json_object *playlists = json_object_new_array();

	playlist_manager_lock( data->my_data->playlist_manager );

	for( Playlist *p = data->my_data->playlist_manager->root; p; p = p->next ) {
		json_object *playlist = json_object_new_object();
		json_object_object_add( playlist, "name", json_object_new_string( p->name ) );
		json_object_object_add( playlist, "id", json_object_new_int( p->id ) );
		json_object *items = json_object_new_array();
		for( PlaylistItem *item = p->root; item != NULL; item = item->next ) {
			json_object *item_obj = json_object_new_object();
			json_object_object_add( item_obj, "path", json_object_new_string( item->path ) );
			json_object_object_add( item_obj, "artist", json_object_new_string( item->artist ) );
			json_object_object_add( item_obj, "title", json_object_new_string( item->title ) );
			json_object_object_add( item_obj, "id", json_object_new_int( item->id ) );
			json_object_array_add( items, item_obj );
		}
		json_object_object_add( playlist, "items", items );

		json_object_array_add( playlists, playlist );
	}

	json_object *root_obj = json_object_new_object();
	json_object_object_add( root_obj, "playlists", playlists );

	playlist_manager_unlock( data->my_data->playlist_manager );

	const char *s = json_object_to_json_string( root_obj );
	struct MHD_Response *response = MHD_create_response_from_buffer( strlen(s), (void*)s, MHD_RESPMEM_MUST_COPY );

	// this causes the json string to be released
	json_object_put( root_obj );

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
		const sds request_body,
		void **con_cls)
{
	Album *p;
	LOG_DEBUG("in web_handler_albums");
	json_object *albums = json_object_new_array();

	struct sglib_Album_iterator it;
	for( p = sglib_Album_it_init_inorder(&it, data->my_data->album_list->root); p != NULL; p = sglib_Album_it_next(&it) ) {
		json_object *album = json_object_new_object();
		json_object_object_add( album, "path", json_object_new_string( p->path ) );
		json_object_object_add( album, "artist", json_object_new_string( p->artist ) );
		json_object_object_add( album, "album", json_object_new_string( p->album ) );

		json_object *tracks = json_object_new_array();
		for( Track *t = p->tracks; t != NULL; t = t->next_ptr ) {
			json_object *track = json_object_new_object();
			json_object_object_add( track, "title", json_object_new_string( t->title ) );
			json_object_object_add( track, "path", json_object_new_string( t->path ) );
			json_object_array_add( tracks, track );
		}
		json_object_object_add( album, "tracks", tracks );

		json_object_array_add( albums, album );
	}

	json_object *streams = json_object_new_array();
	for( Stream *p = data->my_data->stream_list->p; p != NULL; p = p->next_ptr ) {
		json_object_array_add( streams, json_object_new_string( p->name ) );
	}

	json_object *root_obj = json_object_new_object();
	json_object_object_add( root_obj, "albums", albums );
	json_object_object_add( root_obj, "streams", streams );

	const char *s = json_object_to_json_string( root_obj );
	struct MHD_Response *response = MHD_create_response_from_buffer( strlen(s), (void*)s, MHD_RESPMEM_MUST_COPY );

	// this causes the json string to be released
	json_object_put( root_obj );

	int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
	MHD_destroy_response(response);
	return ret;
}

static int web_handler_play(
		WebHandlerData *data,
		struct MHD_Connection *connection,
		const char *url,
		const char *method,
		const char *version,
		const sds request_body,
		void **con_cls)
{
	//int res;
	const char *stream = MHD_lookup_connection_value( connection, MHD_GET_ARGUMENT_KIND, "stream" );
	if( stream ) {
		LOG_DEBUG("stream=s playing stream", stream);
		goto done;
	}

	const char *album = MHD_lookup_connection_value( connection, MHD_GET_ARGUMENT_KIND, "album" );
	if( stream ) {
		LOG_DEBUG("album=s playing album", album);
		goto done;
	}

	const char *playlist = MHD_lookup_connection_value( connection, MHD_GET_ARGUMENT_KIND, "playlist" );
	if( stream ) {
		LOG_DEBUG("playlist=s playing playlist", playlist);
		goto done;
	}
	
	//if( id_str ) {
	//	LOG_DEBUG("id=s got id", id_str);
	//	errno = 0;
	//	long int id = strtol( id_str, NULL, 10 );
	//	if( !errno ) {
	//		LOG_DEBUG("id=d got id", id);
	//		PlaylistItem *x;
	//		res = playlist_manager_get_item_by_id( data->my_data->playlist_manager, id, &x );
	//		if( res == OK ) {
	//			player_change_track( data->my_data->player, x, TRACK_CHANGE_IMMEDIATE );
	//		}
	//	}
	//}

done:
	struct MHD_Response *response = MHD_create_response_from_buffer( 2, "ok", MHD_RESPMEM_PERSISTENT );
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
		const sds request_body,
		void **con_cls)
{
	//const char *id = MHD_lookup_connection_value( connection, MHD_GET_ARGUMENT_KIND, "album" );

	//if( id != NULL ) {
	//	errno = 0;
	//	long int i = strtol(id, NULL, 10);
	//	bool ok = !errno;

	//	if( ok ) {
	//		//if( 0 <= i && i < data->album_list->len ) {
	//		//	// TODO error handling
	//		//	load_quick_album( data->playlist_manager, data->album_list->list[i].path );
	//		//}
	//	}
	//}

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

	RequestSession *request_session;
	if( *con_cls == NULL ) {
		request_session = malloc(sizeof(RequestSession));
		request_session->body = sdsempty();
		*con_cls = (void*) request_session;
		return MHD_YES;
	}

	request_session = (RequestSession*) *con_cls;
	if( *upload_data_size ) {
		request_session->body = sdscatlen( request_session->body, upload_data, *upload_data_size );
		*upload_data_size = 0;
		return MHD_YES;
	}

	// TODO free memory for request_session and request_session->body
	*con_cls = NULL; // reset to signal we are done

	if( strcmp( method, "GET" ) == 0 && strcmp(url, "/websocket") == 0 ) {
		return web_handler_websocket( data, connection, url, method, version, request_session->body, con_cls );
	}

	if( strcmp( method, "GET" ) == 0 && strcmp(url, "/library") == 0 ) {
		return web_handler_albums( data, connection, url, method, version, request_session->body, con_cls );
	}

	//if( strcmp( method, "GET" ) == 0 && strcmp(url, "/playlists") == 0 ) {
	//	return web_handler_playlists( data, connection, url, method, version, request_session->body, con_cls );
	//}

	//if( strcmp( method, "POST" ) == 0 && strcmp(url, "/playlists/load") == 0 ) {
	//	return web_handler_playlists_load( data, connection, url, method, version, request_session->body, con_cls );
	//}

	//if( strcmp( method, "DELETE" ) == 0 && strcmp(url, "/playlists") == 0 ) {
	//	return web_handler_playlists_delete( data, connection, url, method, version, request_session->body, con_cls );
	//}

	//if( strcmp( method, "POST" ) == 0 && strcmp(url, "/playlists") == 0 ) {
	//	return web_handler_playlists_play( data, connection, url, method, version, request_session->body, con_cls );
	//}

	//if( strcmp( method, "POST" ) == 0 && strcmp(url, "/albums") == 0 ) {
	//	return web_handler_albums_play( data, connection, url, method, version, request_session->body, con_cls );
	//}

	if( strcmp( method, "POST" ) == 0 && strcmp(url, "/play") == 0 ) {
		return web_handler_play( data, connection, url, method, version, request_session->body, con_cls );
	}

	if( strcmp( method, "GET" ) == 0 && has_prefix(url, "/static/") ) {
		return web_handler_static( data, connection, url, method, version, request_session->body, con_cls );
	}

	if( strcmp( method, "GET" ) == 0 && strcmp(url, "/") == 0 ) {
		url = "/static/index.html";
		return web_handler_static( data, connection, url, method, version, request_session->body, con_cls );
	}

	return error_handler( connection, "unable to dispatch url: %s", url );
}

int init_http_server_data( WebHandlerData *data, MyData *my_data )
{
	data->my_data = my_data;
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
