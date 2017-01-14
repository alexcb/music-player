#include "player.h"
#include "icy.h"
#include "log.h"

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

	size_t decoded_size;
	size_t buffer_free;

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
		res = open_file( path, &fd );
		if( res ) {
			LOG_ERROR( "unable to open" );
		}

		playlist_manager_unlock( player->playlist_manager );

		if( mpg123_open_fd( player->mh, fd ) != MPG123_OK ) {
			close( fd );
			player->reading_index++;
			continue;
		}

		LOG_DEBUG( "size=d requesting space", sizeof(struct id_data) + 1 );
		for(;;) {
			res = get_buffer_write( &player->circular_buffer, sizeof(struct id_data) + 1, &p, &buffer_free );
			if( !res ) {
				break;
			}
			usleep(100);
		}

		LOG_DEBUG( "writing metadata to buffer" );
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
		}
		buffer_mark_written( &player->circular_buffer, sizeof(struct id_data) + 1 );

		size_t min_buffer_size = mpg123_outblock( player->mh ) + 1 + sizeof(size_t);

		bool done = false;
		bool oom_msg = false;
		while( !done ) {
			res = get_buffer_write( &player->circular_buffer, min_buffer_size, &p, &buffer_free );
			if( res ) {
				if( !oom_msg ) {
					LOG_DEBUG("requested=d out of buffer memory", min_buffer_size);
					oom_msg = true;
				}
				usleep(100);
				continue;
			}
			oom_msg = false;

			*((unsigned char*)p) = AUDIO_DATA;
			p++;
			buffer_free--;

			// reserve some space for number of bytes decoded
			size_t *decoded_size = (size_t*) p;
			p += sizeof(size_t);
			buffer_free -= sizeof(size_t);

			LOG_DEBUG("buffer_free=d decoding mp3", buffer_free);
			res = mpg123_read( player->mh, p, buffer_free, decoded_size);
			switch( res ) {
				case MPG123_OK:
					LOG_DEBUG("size=d wrote to buffer", *decoded_size);
					break;
				case MPG123_NEW_FORMAT:
					LOG_DEBUG("size=d TODO handle new format", *decoded_size);
					break;
				case MPG123_DONE:
					LOG_DEBUG("size=d done", *decoded_size);
					done = true;
					break;
				default:
					printf("non-handled -- mpg123_read returned: %s\n", mpg123_plain_strerror(res));
					break;
			}
			if( *decoded_size > 0 ) {
				buffer_mark_written( &player->circular_buffer, *decoded_size + 1 + sizeof(size_t) );
			}
		}
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

	printf("player running\n");
	for(;;) {
		res = get_buffer_read( &player->circular_buffer, &p, &buffer_avail );
		if( res ) {
			usleep(100);
			continue;
		}

		unsigned char *q = (unsigned char*) p;
		buffer_mark_read( &player->circular_buffer, 1 );
		p++;

		if( *q == ID_DATA ) {
			struct id_data *id_data = (struct id_data*) p;
			LOG_DEBUG( "artist=s title=s playing new track", id_data->artist, id_data->title );
			buffer_mark_read( &player->circular_buffer, sizeof(struct id_data) );
			continue;
		}

		// otherwise it must be audio data
		assert( *q == AUDIO_DATA );

		size_t *decoded_size = (size_t*) p;
		p += sizeof(size_t);
		assert( *decoded_size <= buffer_avail );

		buffer_avail = *decoded_size;
		buffer_mark_read( &player->circular_buffer, sizeof(size_t) );

		chunk_size = 10240;
		while( buffer_avail > 0 ) {
			if( buffer_avail < chunk_size ) {
				chunk_size = buffer_avail;
			}
			ao_play( player->dev, p, chunk_size );
			p += buffer_avail;
			buffer_avail -= chunk_size;
			buffer_mark_read( &player->circular_buffer, chunk_size );
		}
	}
}

		//
		//if( !player->playing ) {
		//	printf("paused\n");
		//	sleep(1);
		//	continue;
		//}
		////if( player->playlist == NULL ) {
		////	printf("no playlist\n");
		////	sleep(1);
		////	continue;
		////}
		//int fd;
		//off_t icy_interval;
		//char *playlist_name;
		//res = playlist_manager_open_fd( player->playlist_manager, &fd, &icy_interval, &playlist_name );
		//if( res ) {
		//	printf("no fd returned\n");
		//	sleep(1);
		//	continue;
		//}
		//printf("opening %d\n", fd);

		//if(MPG123_OK != mpg123_param( player->mh, MPG123_ICY_INTERVAL, icy_interval, 0 )) {
		//	LOG_ERROR( "unable to set icy interval" );
		//	close( fd );
		//	free( playlist_name );
		//	return;
		//}

		//if( mpg123_open_fd( player->mh, fd ) != MPG123_OK ) {
		//	printf("failed to open\n");
		//	close( fd );
		//	free( playlist_name );
		//	continue;
		//}

		//size_t buffer_size = mpg123_outblock( player->mh );
		//unsigned char *buffer = (unsigned char*) malloc(buffer_size);

		//size_t decoded_size;
		//printf("decoding\n");
		//bool done = false;
		//bool last_playing_state = !player->playing;
		//while( !done ) {
		//	if( last_playing_state != player->playing ) {
		//		call_observers( player, playlist_name );
		//		last_playing_state = player->playing;
		//	}
		//	if( !player->playing ) {
		//		// TODO pass along a pause state
		//		// TODO might need to close off internet streams instead of pausing
		//		// depending on what type of fd we are reading from
		//		usleep(100);
		//		continue;
		//	}
		//	if( player->restart ) {
		//		//usleep(100);
		//		player->restart = false;
		//		break;
		//	}
		//	res = mpg123_read( player->mh, buffer, buffer_size, &decoded_size);
		//	switch( res ) {
		//		case MPG123_OK:
		//			ao_play( player->dev, buffer, decoded_size );
		//			if( get_text( player ) ) {
		//				call_observers( player, playlist_name );
		//			}
		//			continue;
		//		case MPG123_NEW_FORMAT:
		//			printf("TODO handle new format\n");
		//			break;
		//		case MPG123_DONE:
		//			done = true;
		//			break;
		//		default:
		//			printf("non-handled -- mpg123_read returned: %s\n", mpg123_plain_strerror(res));
		//			break;
		//	}
		//}
		//strncpy( player->artist, "", PLAYER_ARTIST_LEN );
		//strncpy( player->title, "", PLAYER_TITLE_LEN );
		//close( fd );
		//free( playlist_name );
//	}

//
//	ao_device *dev;
//	ao_sample_format format;
//	int channels, encoding;
//	long rate;
//
//	mpg123_handle *mh;
//	unsigned char *buffer;
//	int copied;
//	size_t buffer_size;
//	size_t done;
//	int err;
//
//	pthread_t thread;
//
//	struct httpdata hd;
//
//
//
//	mh = mpg123_new( NULL, &err );
//	buffer_size = mpg123_outblock( mh );
//	buffer = (unsigned char*) malloc( buffer_size );
//
//	int fd = -1;
//
//	const char *url = "http://streaming.witr.rit.edu:8000/witr-undrgnd-mp3-192";
//
//	if( strstr(url, "http://") ) {
//		fd = http_open(url, &hd);
//		printf("setting icy %ld\n", hd.icy_interval);
//		printf("here5\n");
//		if(MPG123_OK != mpg123_param(mh, MPG123_ICY_INTERVAL, hd.icy_interval, 0)) {
//			printf("unable to set icy interval\n");
//			return;
//		}
//		if(mpg123_open_fd(mh, fd) != MPG123_OK) {
//			printf("error\n");
//			return;
//		}
//	}
//	printf("here6\n");
//	mpg123_getformat(mh, &rate, &channels, &encoding);
//
//	/* set the output format and open the output device */
//	format.bits = mpg123_encsize(encoding) * BITS;
//	format.rate = rate;
//	format.channels = channels;
//	format.byte_format = AO_FMT_NATIVE;
//	format.matrix = 0;
//	printf("here7\n");
//	dev = ao_open_live(player->driver, &format, NULL);
//
//	mpg123_id3v1 *v1;
//	mpg123_id3v2 *v2;
//	char *icy_meta;
//
//	/* decode and play */
//	printf("here8\n");
//	while (mpg123_read(mh, buffer, buffer_size, &done) == MPG123_OK) {
//		int meta = mpg123_meta_check(mh);
//		if( meta & MPG123_NEW_ID3 ) {
//			if( mpg123_id3(mh, &v1, &v2) == MPG123_OK ) {
//				//printf("got meta\n");
//				//if( v1 != NULL ) {
//				//	printf("v1 title: %s\n", v1->title);
//				//	printf("v1 artist: %s\n", v1->artist);
//				//	sprintf(text_buf, "%s: %s - %s", playlist_name, v1->artist, v1->title);
//				//	printf("v1 text: %s\n", text_buf);
//				//	writeText(&lcd_state, text_buf);
//				//}
//				//if( v2 != NULL ) {
//				//	printf("v2 title: %s\n", v2->title->p);
//				//	printf("v2 artist: %s\n", v2->artist->p);
//				//	sprintf(text_buf, "%s: %s - %s", playlist_name, v2->artist->p, v2->title->p);
//				//	printf("v2 text: %s\n", text_buf);
//				//	writeText(&lcd_state, text_buf);
//				//}
//			}
//		}
//		if( meta & MPG123_NEW_ICY ) {
//			if( mpg123_icy(mh, &icy_meta) == MPG123_OK ) {
//				//printf("got ICY: %s\n", icy_meta);
//				//parseICY(icy_meta, playlist_name, text_buf);
//				//writeText(&lcd_state, text_buf);
//
//			}
//		}
//
//		ao_play(dev, (char *) buffer, done);
//
//		//if( !playing || selected_playlist != playing_playlist )
//		//	break;
//	}
//
//	mpg123_close(mh);
//	mpg123_delete(mh);
//	if( fd >= 0 ) {
//		close(fd);
//		fd = -1;
//	}
//
//	free(buffer);
//	ao_close(dev);
//}



int stop_player( Player *player )
{
	mpg123_exit();
	ao_shutdown();
}
