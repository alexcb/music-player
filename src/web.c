#include "web.h"

#include "httpget.h"
#include "player.h"
#include "albums.h"
#include "playlist_manager.h"
#include "string_utils.h"
#include "log.h"
#include "errors.h"
#include "my_data.h"

#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <microhttpd.h>
#include <json-c/json.h>

void update_metadata_web_clients(bool playing, const char *playlist_name, const char *artist, const char *title, void *data)
{
	LOG_DEBUG( "playlist=s artist=s title=s update_metadata_web_clients", playlist_name, artist, title );
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
		MyData *data,
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

	FILE *file = fopen( url, "rb" );
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

static int web_handler_albums(
		MyData *data,
		struct MHD_Connection *connection,
		const char *url,
		const char *method,
		const char *version,
		const char *upload_data,
		size_t *upload_data_size,
		void **con_cls)
{

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
		MyData *data,
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
	MyData *data = (MyData*) cls;

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


//	if( strcmp(url, "albums") == 0 ) {
//		
//	}
//	Player *player = (Player*) cls;
//	static int old_connection_marker;
//	int new_connection = (NULL == *con_cls);
//
//	if (new_connection)
//	{
//		/* new connection with POST */
//		*con_cls = &old_connection_marker;
//		printf("got new connection: %s\n", url);
//		return MHD_YES;
//	}
//
//
//	const char *value = MHD_lookup_connection_value( connection, MHD_GET_ARGUMENT_KIND, "q" );
//
//	if( value == NULL ) {
//		value = "NOTHING";
//	}
//	printf("resume connection: %s\n", value);
//
//	player->playing = !player->playing;
//
//	struct MHD_Response *response = MHD_create_response_from_buffer(5, "hello", MHD_RESPMEM_PERSISTENT);
//
//	int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
//	MHD_destroy_response(response);
//	return ret;
}

int start_http_server( MyData *data )
{
	struct MHD_Daemon *d = MHD_start_daemon(
			MHD_USE_SELECT_INTERNALLY,
			80,
			NULL,
			NULL,
			&web_handler,
			(void*) data,
			MHD_OPTION_END);

	if( d == NULL ) {
		return 1;
	}


	LOG_DEBUG("busy looping");
	//for(;;) { sleep(10); printf("busy loop sleep\n"); }
	for(;;) { sleep(10); }
}
