#include "player.h"
#include "icy.h"
#include "log.h"
#include "io_utils.h"

#include <string.h>
#include <math.h>

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <unistd.h>
#include <err.h>
#include <assert.h>

#include <ao/ao.h>

#define AUDIO_DATA 1
#define ID_DATA 2

struct id_data {
	char artist[PLAYER_ARTIST_LEN];
	char title[PLAYER_TITLE_LEN];
};

#define BITS 8

void player_audio_thread_run( void *data );
void player_reader_thread_run( void *data );

int start_player( Player *player )
{
	int res;

	ao_initialize();
	player->driver = ao_default_driver_id();

	player->format.bits = 16;
	player->format.rate = 44100;
	player->format.channels = 2;
	player->format.byte_format = AO_FMT_NATIVE;
	player->format.matrix = NULL;

	player->dev = ao_open_live( player->driver, &player->format, NULL );

	mpg123_init();
	player->mh = mpg123_new( NULL, NULL );

	player->playing = true;
	player->restart = false;

	player->playing_index = 0;
	player->reading_index = 0;
	player->track_change_mode = TRACK_CHANGE_IMMEDIATE;
	player->next_track = NULL;

	res = init_circular_buffer( &player->circular_buffer, 10*1024*1024 );
	if( res ) {
		goto error;
	}

	res = pthread_create( &player->audio_thread, NULL, (void *) &player_audio_thread_run, (void *) player);
	if( res ) {
		goto error;
	}

	res = pthread_create( &player->reader_thread, NULL, (void *) &player_reader_thread_run, (void *) player);
	if( res ) {
		goto error;
	}

	return 0;

error:
	//free( player->buffer );
	//mpg123_exit();
	//ao_shutdown();
	return 1;
}

void call_observers( Player *player, const char *playlist_name ) {
	for( int i = 0; i < player->num_metadata_observers; i++ ) {
		player->metadata_observers[i]( player->playing, playlist_name, player->artist, player->title, player->metadata_observers_data[i] );
	}
}

bool get_text( Player *player ) {
	mpg123_id3v1 *v1;
	mpg123_id3v2 *v2;
	char *icy_meta;
	int res;

	int meta = mpg123_meta_check( player->mh );
	if( meta & MPG123_NEW_ID3 ) {
		if( mpg123_id3( player->mh, &v1, &v2) == MPG123_OK ) {
			if( v2 != NULL ) {
				strncpy( player->artist, v2->artist->p, PLAYER_ARTIST_LEN );
				strncpy( player->title, v2->title->p, PLAYER_TITLE_LEN );
				return true;
			} else if( v1 != NULL ) {
				strncpy( player->artist, v1->artist, PLAYER_ARTIST_LEN );
				strncpy( player->title, v1->title, PLAYER_TITLE_LEN );
				return true;
			}
		}
	}
	if( meta & MPG123_NEW_ICY ) {
		if( mpg123_icy( player->mh, &icy_meta) == MPG123_OK ) {
			printf("got ICY: %s\n", icy_meta);
			char *station;
			res = parse_icy( icy_meta, &station );
			if( res ) {
				LOG_ERROR( "icymeta=s failed to decode icy", icy_meta );
			} else {
				strncpy( player->artist, station, PLAYER_ARTIST_LEN );
				strncpy( player->title, "", PLAYER_TITLE_LEN );
				free( station );
				return true;
			}
		}
	}
	return false;
}

void player_reader_thread_run( void *data )
{
	Player *player = (Player*) data;

	int res;
	int fd;
	off_t icy_interval;
	char *playlist_name;
	const char *path;
	int num_items;
	char *p;
	bool done;
	int playlist_version;

	size_t *decoded_size;
	size_t buffer_free;
	size_t min_buffer_size;
	size_t bytes_written;
	bool is_stream;

restart_reading:
	playlist_manager_lock( player->playlist_manager );
	playlist_version = player->playlist_manager->version;
	playlist_manager_unlock( player->playlist_manager );

	player->reading_index = 0;
	player->new_track = true;

	for(;;) {
		playlist_manager_lock( player->playlist_manager );

		res = playlist_manager_get_length( player->playlist_manager, &num_items );
		if( res ) {
			LOG_ERROR( "unable to get playlist length" );
		}

		if( player->reading_index >= num_items ) {
			player->reading_index = 0;
		}

		res = playlist_manager_get_path( player->playlist_manager, player->reading_index, &path );
		if( res ) {
			LOG_ERROR( "unable to get path" );
		}

		LOG_DEBUG("path=s opening file in reader", path);
		res = open_fd( path, &fd, &is_stream, &icy_interval );
		if( res ) {
			LOG_ERROR( "unable to open" );
		}

		playlist_manager_unlock( player->playlist_manager );

		if( is_stream ) {
			if(MPG123_OK != mpg123_param( player->mh, MPG123_ICY_INTERVAL, icy_interval, 0)) {
				LOG_ERROR( "unable to set icy interval" );
				continue;
			}
		}

		if( mpg123_open_fd( player->mh, fd ) != MPG123_OK ) {
			LOG_ERROR( "mpg123_open_fd failed" );
			close( fd );
			player->reading_index++;
			continue;
		}

		LOG_DEBUG( "size=d requesting space", sizeof(struct id_data) + 1 );
		for(;;) {
			if( playlist_version != player->playlist_manager->version ) {
				mpg123_close( player->mh );
				close( fd );
				goto restart_reading;
			}
			res = get_buffer_write( &player->circular_buffer, sizeof(struct id_data) + 1, &p, &buffer_free );
			if( !res ) {
				break;
			}
			usleep(10);
		}

		if( player->new_track ) {
			player->new_track = false;
			while( __sync_bool_compare_and_swap( &player->next_track, player->next_track, p) == false );
		}

		*((unsigned char*)p) = ID_DATA;
		p++;
		struct id_data *id_data = (struct id_data*) p;
		memset( p, 0, sizeof(struct id_data) );
		
		mpg123_scan( player->mh );

		mpg123_id3v1 *v1;
		mpg123_id3v2 *v2;
		int meta = mpg123_meta_check( player->mh );
		if( meta & MPG123_NEW_ID3 ) {
			if( mpg123_id3( player->mh, &v1, &v2 ) == MPG123_OK ) {
				if( v2 != NULL ) {
					LOG_DEBUG( "populating metadata with id3 v2" );
					strncpy( id_data->artist, v2->artist->p, PLAYER_ARTIST_LEN );
					strncpy( id_data->title, v2->title->p, PLAYER_TITLE_LEN );
				} else if( v1 != NULL ) {
					LOG_DEBUG( "populating metadata with id3 v1" );
					strncpy( id_data->artist, v1->artist, PLAYER_ARTIST_LEN );
					strncpy( id_data->title, v1->title, PLAYER_TITLE_LEN );
				} else {
					assert( false );
				}
			}
		} else {
			LOG_INFO( "no ID3 data available" );
		}
		buffer_mark_written( &player->circular_buffer, sizeof(struct id_data) + 1 );

		min_buffer_size = mpg123_outblock( player->mh ) + 1 + sizeof(size_t);
		done = false;
		while( !done ) {
			if( playlist_version != player->playlist_manager->version ) {
				goto restart_reading;
			}
			res = get_buffer_write( &player->circular_buffer, min_buffer_size, &p, &buffer_free );
			if( res ) {
				usleep(10);
				continue;
			}

			// dont read too much
			buffer_free = min_buffer_size;

			*((unsigned char*)p) = AUDIO_DATA;
			p++;
			buffer_free--;
			bytes_written = 1;

			// reserve some space for number of bytes decoded
			decoded_size = (size_t*) p;
			p += sizeof(size_t);
			buffer_free -= sizeof(size_t);
			bytes_written += sizeof(size_t);

			*decoded_size = 0;

			res = mpg123_read( player->mh, p, buffer_free, decoded_size);
			switch( res ) {
				case MPG123_OK:
					break;
				case MPG123_NEW_FORMAT:
					LOG_DEBUG("TODO handle new format");
					break;
				case MPG123_DONE:
					done = true;
					break;
				default:
					LOG_ERROR("err=s unhandled mpg123 error", mpg123_plain_strerror(res));
					break;
			}
			if( *decoded_size > 0 ) {
				bytes_written += *decoded_size;
				//LOG_DEBUG("size=d wrote decoded data", bytes_written);
				buffer_mark_written( &player->circular_buffer, bytes_written );
			}
		}
		mpg123_close( player->mh );
		close( fd );
		player->reading_index++;
	}
}

void player_audio_thread_run( void *data )
{
	int res;
	Player *player = (Player*) data;

	//play_tone( player );
	//setbuf(stdout, NULL);

	size_t buffer_avail;
	size_t chunk_size;
	char *p;
	char *q;
	char *buf_start;
	char *next_track = NULL;
	char *current_song = NULL;
	unsigned char payload_id;

	for(;;) {
		res = get_buffer_read( &player->circular_buffer, &p, &buffer_avail );
		if( res ) {
			usleep(10);
			continue;
		}
		buf_start = p;

		if( next_track ) {
			LOG_DEBUG( "skipping to next track" );
			if( next_track < p ) {
				LOG_DEBUG( "skip to end of buffer" );
				buffer_mark_read( &player->circular_buffer, buffer_avail );
				continue;
			}
			if( p < next_track ) {
				LOG_DEBUG( "skip to next track" );
				chunk_size = next_track - p;
				assert( chunk_size <= buffer_avail );
				buffer_mark_read( &player->circular_buffer, chunk_size );
				continue;
			}
			LOG_DEBUG( "finished skipping" );
			next_track = NULL;
		}

		payload_id = *(unsigned char*) p;
		p++;
		buffer_avail--;
		buffer_mark_read( &player->circular_buffer, 1 );

		if( payload_id == ID_DATA ) {
			current_song = buf_start;
			struct id_data *id_data = (struct id_data*) p;
			LOG_DEBUG( "artist=s title=s playing new track", id_data->artist, id_data->title );
			buffer_mark_read( &player->circular_buffer, sizeof(struct id_data) );
			continue;
		}

		// otherwise it must be audio data
		assert( payload_id == AUDIO_DATA );

		size_t decoded_size = *((size_t*) p);
		p += sizeof(size_t);
		buffer_avail-= sizeof(size_t);
		buffer_mark_read( &player->circular_buffer, sizeof(size_t) );

		assert( decoded_size <= buffer_avail );

		//size_t decoded_size = buffer_avail;
		chunk_size = 1024;
		while( decoded_size > 0 ) {
			if( player->next_track && player->track_change_mode == TRACK_CHANGE_IMMEDIATE ) {
				next_track = NULL;
				do {
					q = player->next_track;
					next_track = __sync_val_compare_and_swap( &player->next_track, q, NULL );
				} while( next_track != q );

				if( next_track == current_song ) {
					next_track = NULL;
				} else {
					buffer_mark_read( &player->circular_buffer, decoded_size );
					break;
				}
			}

			if( !player->playing ) {
				usleep(10);
				continue;
			}

			if( decoded_size < chunk_size ) {
				chunk_size = decoded_size;
			}
			ao_play( player->dev, p, chunk_size );
			p += chunk_size;
			decoded_size -= chunk_size;
			buffer_mark_read( &player->circular_buffer, chunk_size );
		}
	}
}

int stop_player( Player *player )
{
	mpg123_exit();
	ao_shutdown();
}
