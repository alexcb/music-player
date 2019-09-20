#include "player.h"
#include "icy.h"
#include "io_utils.h"
#include "library.h"
#include "log.h"
#include "my_malloc.h"

#ifdef USE_RASP_PI
#	include "raspbpi.h"
#endif

#include "voice_synth.h"
#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define RATE 44100

void* player_audio_thread_run( void* data );
void* player_reader_thread_run( void* data );
void player_load_into_buffer( Player* player, PlaylistItem* item );

int init_player( Player* player, const char* library_path )
{
	int res;

	memset( player, 0, sizeof( Player ) );

	// controls for the loader
	player->load_item = &player_load_into_buffer;
	player->load_in_progress = false;
	player->load_abort_requested = false;
	player->playlist_item_to_buffer = NULL;
	player->playlist_item_to_buffer_override = NULL;

	// audio player thread variables
	player->control = 0;

	player->current_track = NULL;

	player->library_path = library_path;

#ifdef USE_RASP_PI
	init_alsa( player );
	LOG_INFO( "using alsa" );
	player->audio_consumer = &consume_alsa;
#else
	init_pa( player );
	LOG_INFO( "using pulseaudio" );
	player->audio_consumer = &consume_pa;
#endif

	player->meta_audio_max = 5 * 1024 * 1024;
	player->meta_audio = my_malloc( player->meta_audio_max );
	init_pico();

	mpg123_init();
	player->mh = mpg123_new( NULL, NULL );
	mpg123_format_none( player->mh );
	mpg123_format( player->mh, RATE, MPG123_STEREO, MPG123_ENC_SIGNED_16 );

	player->decode_buffer_size = mpg123_outblock( player->mh );
	player->decode_buffer = my_malloc( player->decode_buffer_size );

	// TODO ensure that ID_DATA_START messages are smaller than this
	player->max_payload_size = mpg123_outblock( player->mh ) + 1 + sizeof( size_t );

	pthread_mutex_init( &player->the_lock, NULL );

	res = pthread_cond_init( &( player->load_cond ), NULL );
	assert( res == 0 );

	res = pthread_cond_init( &( player->done_track_cond ), NULL );
	assert( res == 0 );

	res = init_circular_buffer( &player->circular_buffer, 10 * 1024 * 1024 );
	if( res ) {
		goto error;
	}

	play_queue_init( &player->play_queue );

	player->metadata_observers_num = 0;
	player->metadata_observers_cap = 2;
	player->metadata_observers =
		(MetadataObserver*)my_malloc( sizeof( MetadataObserver ) * player->metadata_observers_cap );
	if( player->metadata_observers == NULL ) {
		goto error;
	}
	player->metadata_observers_data =
		(void**)my_malloc( sizeof( void* ) * player->metadata_observers_cap );
	if( player->metadata_observers_data == NULL ) {
		goto error;
	}
	return 0;

error:
	//my_free( player->buffer );
	//mpg123_exit();
	//ao_shutdown();
	return 1;
}

int player_get_control( Player* player )
{
	int control;
	pthread_mutex_lock( &player->the_lock );
	control = player->control;
	pthread_mutex_unlock( &player->the_lock );
	return control;
}

int start_player( Player* player )
{
	int res;
	res = pthread_create( &player->audio_thread, NULL, &player_audio_thread_run, (void*)player );
	if( res ) {
		goto error;
	}

	res = pthread_create( &player->reader_thread, NULL, &player_reader_thread_run, (void*)player );
	if( res ) {
		goto error;
	}
	return 0;

error:
	return 1;
}

void stop_loader( Player* player )
{
	int res;
	while( player->load_in_progress ) {
		player->load_abort_requested = true;
		wake_up_get_buffer_write( &player->circular_buffer );
		res = pthread_cond_wait( &player->load_cond, &player->the_lock );
		assert( res == 0 );
	}
}

void start_loader( Player* player )
{
	pthread_mutex_lock( &player->the_lock );
	player->load_abort_requested = false;
	pthread_mutex_unlock( &player->the_lock );
}

int player_change_track_unsafe( Player* player, PlaylistItem* playlist_item, int when )
{
	assert( when == TRACK_CHANGE_IMMEDIATE );
	stop_loader( player );
	player->playlist_item_to_buffer_override = playlist_item;
	player->control |= PLAYER_CONTROL_SKIP;
	return 0;
}

int player_change_next_album( Player* player, int when )
{
	int res = 1;
	PlaylistItem* p;

	pthread_mutex_lock( &player->the_lock );

	if( player->current_track != NULL ) {
		for( p = player->current_track; p != NULL; p = p->next ) {
			if( player->current_track->track == NULL || p->track == NULL ) {
				break;
			}
			if( strcmp( player->current_track->track->album, p->track->album ) != 0 ) {
				break;
			}
		}
		if( p == NULL ) {
			// start back at the begining
			p = player->current_track->parent->root;
		}

		res = player_change_track_unsafe( player, p, when );
	}

	pthread_mutex_unlock( &player->the_lock );
	return res;
}

int player_change_next_track( Player* player, int when )
{
	int res = 1;
	PlaylistItem* p;

	pthread_mutex_lock( &player->the_lock );

	if( player->current_track != NULL ) {
		p = player->current_track->next;
		if( p == NULL ) {
			p = player->current_track->parent->root;
		}
		assert( p );

		res = player_change_track_unsafe( player, p, when );
	}

	pthread_mutex_unlock( &player->the_lock );
	return res;
}

int player_change_prev_track( Player* player, int when )
{
	int res = 1;
	PlaylistItem* prev = NULL;
	PlaylistItem* p = NULL;

	pthread_mutex_lock( &player->the_lock );

	p = player->current_track->parent->root;
	while( p != NULL ) {
		if( p == player->current_track ) {
			break;
		}
		prev = p;
		p = p->next;
	}

	if( prev != NULL ) {
		res = player_change_track_unsafe( player, prev, when );
	}
	else if( p != NULL ) {
		res = player_change_track_unsafe( player, p, when );
	}

	pthread_mutex_unlock( &player->the_lock );
	return res;
}

int player_change_next_playlist( Player* player, int when )
{
	int res = 1;
	Playlist* pl;
	Playlist* initial_playlist;
	PlaylistItem* p;

	pthread_mutex_lock( &player->the_lock );

	if( player->current_track != NULL ) {
		pl = player->current_track->parent;
	}
	else {
		LOG_WARN( "no current track; setting default playlist as active playlist" );
		pl = player->playlist_manager->root;
		if( pl == NULL ) {
			LOG_ERROR( "no playlists" );
			goto error;
		}
	}
	assert( pl );
	initial_playlist = pl;

	for( ;; ) {
		assert( pl->next );
		pl = pl->next;
		LOG_DEBUG( "playlist=s root=p playlist", pl->name, pl->root );
		if( pl->root ) {
			break;
		}
		if( pl == initial_playlist ) {
			break;
		}
	}
	if( pl->current ) {
		p = pl->current;
	}
	else {
		p = pl->root;
	}
	if( p == NULL ) {
		LOG_WARN( "unable to change playlist; no playlists contain any tracks" );
		res = 1;
		goto error;
	}

	LOG_INFO( "playlist=s root=p setting next playlist", pl->name, pl->root );
	res = player_change_track_unsafe( player, p, when );

error:
	pthread_mutex_unlock( &player->the_lock );
	return res;
}

void player_say_what( Player* player )
{
	pthread_mutex_lock( &player->the_lock );

	player->say_track_info = true;

	pthread_mutex_unlock( &player->the_lock );
}

int player_change_track( Player* player, PlaylistItem* playlist_item, int when )
{
	int res;
	if( when != TRACK_CHANGE_IMMEDIATE && when != TRACK_CHANGE_NEXT ) {
		return 1;
	}

	pthread_mutex_lock( &player->the_lock );

	res = player_change_track_unsafe( player, playlist_item, when );

	pthread_mutex_unlock( &player->the_lock );

	return res;
}

int player_notify_item_change( Player* player, PlaylistItem* playlist_item )
{
	// TODO trigger a reload buffer event if required
	return 0;
}

int player_add_metadata_observer( Player* player, MetadataObserver observer, void* data )
{
	if( player->metadata_observers_num == player->metadata_observers_cap ) {
		return 1;
	}
	player->metadata_observers[player->metadata_observers_num] = observer;
	player->metadata_observers_data[player->metadata_observers_num] = data;
	player->metadata_observers_num++;
	return 0;
}

void call_observers( Player* player )
{
	bool is_playing = player_get_control( player ) & PLAYER_CONTROL_PLAYING;
	int playlist_version = playlist_manager_checksum( player->playlist_manager );
	for( int i = 0; i < player->metadata_observers_num; i++ ) {
		player->metadata_observers[i]( is_playing,
									   player->current_track,
									   playlist_version,
									   player->metadata_observers_data[i] );
	}
}

bool player_should_abort_load( Player* player )
{
	bool b = false;
	pthread_mutex_lock( &player->the_lock );
	b = player->load_abort_requested;
	pthread_mutex_unlock( &player->the_lock );
	return b;
}

void player_load_into_buffer( Player* player, PlaylistItem* item )
{
	bool done;
	int res;
	char* p;
	int fd = 0;
	size_t buffer_free;
	size_t decoded_size;

	bool is_stream = false;
	off_t icy_interval;
	char* icy_meta;
	char* icy_title;
	char* icy_name = NULL;
	size_t bytes_written;

	sds full_path = NULL;

	PlayQueueItem* pqi = NULL;
	const char* path;

	float volume = 1.0f;

	if( item->track != NULL ) {
		path = sdscatfmt( NULL, "%s/%s", player->library_path, item->track->path );
		volume = 1.0f;
	}
	else if( item->stream != NULL ) {
		path = item->stream->url;
		volume = item->stream->volume;
		is_stream = true;
	}
	else {
		assert( 0 );
	}

	// dont start loading a stream if paused
	if( is_stream ) {
		for( ;; ) {
			int control = player_get_control( player );
			if( control & PLAYER_CONTROL_PLAYING ) {
				break;
			}
			usleep( 10000 );
			if( player_should_abort_load( player ) ) {
				goto done;
			}
		}
		res = open_stream( path, &fd, &icy_interval, &icy_name );
		if( res ) {
			LOG_ERROR( "unable to open stream" );
			goto done;
		}
	}
	else {
		icy_interval = 0;

		fd = open( path, O_RDONLY );
		if( fd < 0 ) {
			LOG_ERROR( "unable to open" );
			goto done;
		}
	}

	mpg123_param( player->mh, MPG123_ICY_INTERVAL, icy_interval, 0 );

	if( mpg123_open_fd( player->mh, fd ) != MPG123_OK ) {
		LOG_ERROR( "mpg123_open_fd failed" );
		goto done;
	}

	// acquire an empty play queue item (busy-wait until one is free)
	for( ;; ) {
		pthread_mutex_lock( &player->the_lock );

		res = get_buffer_write(
			&player->circular_buffer, player->max_payload_size, &p, &buffer_free );
		if( res ) {
			pthread_mutex_unlock( &player->the_lock );
			if( player_should_abort_load( player ) ) {
				goto done;
			}
		}

		//TODO play queue needs conditional waits
		res = play_queue_add( &player->play_queue, &pqi );
		if( res ) {
			pthread_mutex_unlock( &player->the_lock );

			if( player_should_abort_load( player ) ) {
				goto done;
			}
			usleep( 50000 );
			continue;
		}

		pqi->buf_start = p;
		pqi->playlist_item = item;
		playlist_item_ref_up( item );
		pqi = NULL;

		pthread_mutex_unlock( &player->the_lock );
		break;
	}

	*( (unsigned char*)p ) = ID_DATA_START;
	p++;
	if( icy_name ) {
		LOG_DEBUG( "icy=s TODO something with icy name", icy_name );
	}

	buffer_mark_written( &player->circular_buffer, 1 );
	p = NULL;

	done = false;
	while( !done ) {
		int control = player_get_control( player );
		if( player_should_abort_load( player ) ) {
			goto done;
		}

		if( !( control & PLAYER_CONTROL_PLAYING ) && is_stream ) {
			// can't pause a stream; just quit
			break;
		}

		// TODO ensure there is enough room for audio + potential ICY data; FIXME for now just multiply by 2
		res = get_buffer_write(
			&player->circular_buffer, player->max_payload_size * 2, &p, &buffer_free );
		if( res ) {
			usleep( 50000 );
			continue;
		}

		bytes_written = 0;
		res = mpg123_read( player->mh,
						   (unsigned char*)player->decode_buffer,
						   player->decode_buffer_size,
						   &decoded_size );
		switch( res ) {
		case MPG123_OK:
			break;
		case MPG123_NEW_FORMAT:
			LOG_DEBUG( "TODO handle new format" );
			break;
		case MPG123_DONE:
			done = true;
			break;
		default:
			LOG_ERROR( "err=s unhandled mpg123 error", mpg123_plain_strerror( res ) );
			break;
		}
		int meta = mpg123_meta_check( player->mh );
		if( meta & MPG123_NEW_ICY ) {
			if( mpg123_icy( player->mh, &icy_meta ) == MPG123_OK ) {
				res = parse_icy( icy_meta, &icy_title );
				if( res ) {
					LOG_ERROR( "icymeta=s failed to decode icy", icy_meta );
				}
				else {
					*( (unsigned char*)p ) = ID_DATA_START;
					p++;

					LOG_DEBUG( "icymeta=s TODO something with icy title", icy_title );

					bytes_written += 1;

					//memset( track_info, 0, sizeof(PlayerTrackInfo) );
					//track_info->playlist_item = NULL;
					//track_info->is_stream = is_stream;
					//strcpy( track_info->artist, "" );
					//strcpy( track_info->title, icy_title );
					//strcpy( track_info->icy_name, icy_name );
					my_free( icy_title );
				}
			}
		}

		if( volume != 1.0f ) {
			bool clipped = false;
			// only supports 16bit
			for( int i = 0; i < decoded_size; i += 2 ) {
				int16_t* p = (int16_t*)( player->decode_buffer + i );
				float y = ( (float)*p ) * volume;
				if( y > 32760 ) {
					clipped = true;
				}
				*p = y;
			}
			if( clipped ) {
				LOG_WARN( "clipped audio, volume is too high" );
			}
		}

		if( decoded_size > 0 ) {
			*( (unsigned char*)p ) = AUDIO_DATA;
			p++;
			bytes_written += 1;
			buffer_free--;

			// reserve some space for number of bytes decoded
			*( (size_t*)p ) = decoded_size;
			p += sizeof( size_t );
			buffer_free -= sizeof( size_t );
			bytes_written += sizeof( size_t );

			memcpy( p, player->decode_buffer, decoded_size );
			p += decoded_size;
			bytes_written += decoded_size;
		}

		if( bytes_written > 0 ) {
			buffer_mark_written( &player->circular_buffer, bytes_written );
		}
	}

	*( (unsigned char*)p ) = ID_DATA_END;
	p++;
	buffer_mark_written( &player->circular_buffer, 1 );

done:
	// no easy way to see if is open, but closing a closed reader is fine.
	mpg123_close( player->mh );
	if( fd ) {
		close( fd );
	}
	if( full_path ) {
		sdsfree( full_path );
	}
}

// caller must first lock the player before calling this
void player_rewind_buffer_unsafe( Player* player )
{
	int res;
	PlayQueueItem* pqi = NULL;

	res = play_queue_head( &player->play_queue, &pqi );
	if( !res ) {
		buffer_lock( &player->circular_buffer );
		buffer_rewind_unsafe( &player->circular_buffer, pqi->buf_start );
		buffer_unlock( &player->circular_buffer );
	}
	play_queue_clear( &player->play_queue );
}

void* player_reader_thread_run( void* data )
{
	Player* player = (Player*)data;
	PlaylistItem* item = NULL;

	for( ;; ) {
		pthread_mutex_lock( &player->the_lock );

		if( player->control & PLAYER_CONTROL_EXIT ) {
			pthread_mutex_unlock( &player->the_lock );
			return NULL;
		}

		if( player->load_abort_requested ) {
			// loader has been told to stop, busy-wait here
			pthread_mutex_unlock( &player->the_lock );
			usleep( 100 );
			continue;
		}

		if( player->playlist_item_to_buffer_override != NULL ) {
			player->playlist_item_to_buffer = player->playlist_item_to_buffer_override;
			player->playlist_item_to_buffer_override = NULL;
		}

		if( player->playlist_item_to_buffer != NULL ) {
			item = player->playlist_item_to_buffer;
			playlist_item_ref_up( item );
			player->load_in_progress = true;
			player->load_abort_requested = false;
		}
		else {
			item = NULL;
		}

		pthread_mutex_unlock( &player->the_lock );

		if( item == NULL ) {
			usleep( 10000 );
			continue;
		}

		player->load_item( player, item );

		pthread_mutex_lock( &player->the_lock );

		playlist_item_ref_down( item );
		item = NULL;

		if( player->playlist_item_to_buffer != NULL ) {
			if( player->playlist_item_to_buffer->next ) {
				player->playlist_item_to_buffer = player->playlist_item_to_buffer->next;
			}
			else if( player->playlist_item_to_buffer->parent == NULL ) {
				player->playlist_item_to_buffer = NULL;
			}
			else {
				player->playlist_item_to_buffer = player->playlist_item_to_buffer->parent->root;
			}
		}

		player->load_in_progress = false;
		pthread_mutex_unlock( &player->the_lock );
		pthread_cond_signal( &player->load_cond );
		usleep( 100 );
	}
	return NULL;
}

void player_lock( Player* player )
{
	pthread_mutex_lock( &player->the_lock );
}
void player_unlock( Player* player )
{
	pthread_mutex_unlock( &player->the_lock );
}

void* player_audio_thread_run( void* data )
{
	int res;
	int control;
	int last_control = 19432;
	Player* player = (Player*)data;

	//size_t chunk_size;
	//unsigned char payload_id;

	PlayQueueItem* pqi = NULL;
	//PlaylistItem *playlist_item = NULL;

	unsigned char payload_id;

	size_t num_read;
	size_t buffer_avail;
	char* p;
	bool is_stream = false;
	bool notified_no_songs = false;

	for( ;; ) {
		pthread_mutex_lock( &player->the_lock );

		if( player->control & PLAYER_CONTROL_SKIP ) {
			player->control &= ~PLAYER_CONTROL_SKIP;

			// TODO ideally this should happen elsewhere in a different thread (need to rewind instead of clear)
			play_queue_clear( &player->play_queue );
			buffer_clear( &player->circular_buffer );

			// start the loader back up
			player->load_abort_requested = false;

			// unlock, sleep, then continue -- to give the loader time to run
			pthread_mutex_unlock( &player->the_lock );
			usleep( 100000 ); //100ms
			continue;
		}

		if( player->current_track ) {
			//playlist_item_ref_down(player->current_track);
			player->current_track = NULL;
		}

		res = play_queue_head( &player->play_queue, &pqi );
		if( res == 0 ) {
			buffer_mark_read_upto( &player->circular_buffer, pqi->buf_start );
		}
		else {
			// Play queue is empty -- nothing to play
			buffer_clear( &player->circular_buffer );

			pthread_mutex_unlock( &player->the_lock );

			if( !notified_no_songs ) {
				call_observers( player );
				notified_no_songs = true;
			}

			usleep( 100000 ); //100ms
			continue;
		}

		player->current_track = pqi->playlist_item;
		player->current_track->parent->current =
			player->current_track; // update playlist to point to current item
		void* buf_start = pqi->buf_start;
		is_stream = ( pqi->playlist_item->stream != NULL );
		assert( buf_start != NULL );
		pqi =
			NULL; //once the play_queue is unlocked, this memory will point to something else, make sure we dont use it.
		play_queue_pop( &player->play_queue );

		pthread_mutex_unlock( &player->the_lock );
		pthread_cond_signal( &player->done_track_cond );

		notified_no_songs = false;

		bool first_start = true;
		LOG_INFO( "track start" );
		for( ;; ) {
			control = player_get_control( player );
			if( control & PLAYER_CONTROL_EXIT ) {
				return NULL;
			}

			num_read = 0;
			res = get_buffer_read( &player->circular_buffer, &p, &buffer_avail );
			if( res ) {
				LOG_WARN( "out of buffer" );
				usleep( 200000 ); // 200ms
				continue;
			}

			assert( buffer_avail > 0 );

			payload_id = *(unsigned char*)p;

			p++;
			num_read++;

			if( payload_id == ID_DATA_END ) {
				buffer_mark_read( &player->circular_buffer, num_read );
				break;
			}

			if( payload_id == ID_DATA_START ) {
				//memcpy( &player->current_track, p, sizeof(PlayerTrackInfo) );
				//player->current_track.playlist_item->parent->current = player->current_track.playlist_item;
				call_observers( player );
				buffer_mark_read( &player->circular_buffer, num_read );
				continue;
			}

			if( player->say_track_info ) {
				char audio_text[1024];
				if( player->current_track && player->current_track->track ) {
					sprintf( audio_text,
							 "the artist is %s. the album is %s'. the track is %s",
							 player->current_track->track->artist,
							 player->current_track->track->album,
							 player->current_track->track->title );
				}
				else {
					sprintf( audio_text, "I have no clue, just enjoy it." );
				}
				LOG_INFO( "text=s n=d synth start", audio_text, player->meta_audio_n );

				player->meta_audio_n =
					synth_text( audio_text, player->meta_audio, player->meta_audio_max );
				char* p = player->meta_audio;
				while( player->meta_audio_n > 0 ) {
					size_t n = player->meta_audio_n;
					if( n > 1024 ) {
						n = 1024;
					}
					player->audio_consumer( player, p, n );
					player->meta_audio_n -= n;
					p += n;
				}

				player->say_track_info = false;
			}

			// otherwise it must be audio data
			assert( payload_id == AUDIO_DATA );

			size_t chunk_size;
			memcpy( &chunk_size, p, sizeof( size_t ) );
			num_read += sizeof( size_t ) + chunk_size;
			p += sizeof( size_t );
			assert( chunk_size < buffer_avail );

			while( chunk_size > 0 ) {
				control = player_get_control( player );
				if( control != last_control ) {
					last_control = control;
					call_observers( player );
				}

				if( control & PLAYER_CONTROL_SKIP ) {
					goto track_done;
				}
				if( !( control & PLAYER_CONTROL_PLAYING ) ) {
					if( is_stream ) {
						break;
					}
					usleep( 1000 );
					continue;
				}

				size_t n = chunk_size;
				if( n > 256 * 8 ) {
					n = 256 * 8;
				}

				if( first_start ) {
					LOG_INFO( "first consume" );
					first_start = false;
				}

				//player->meta_audio_n = synth_text( "hello, this is a test.", player->meta_audio, player->meta_audio_max );
				//player->audio_consumer( player, player->meta_audio, player->meta_audio_n );

				player->audio_consumer( player, p, n );
				p += n;
				chunk_size -= n;
			}
			buffer_mark_read( &player->circular_buffer, num_read );
			num_read = 0;
		}
		LOG_INFO( "track done" );
	track_done:
		pthread_cond_signal( &player->done_track_cond );
	}
	return NULL;
}

void player_set_playing( Player* player, bool playing )
{
#ifdef USE_RASP_PI
	rpi_set_status( playing );
#endif

	pthread_mutex_lock( &player->the_lock );
	if( playing ) {
		player->control |= PLAYER_CONTROL_PLAYING;
	}
	else {
		player->control &= ~PLAYER_CONTROL_PLAYING;
	}
	pthread_mutex_unlock( &player->the_lock );
}

void player_pause( Player* player )
{
	bool is_playing;
	pthread_mutex_lock( &player->the_lock );
	player->control ^= PLAYER_CONTROL_PLAYING;
	is_playing = player->control & PLAYER_CONTROL_PLAYING;
	pthread_mutex_unlock( &player->the_lock );
	rpi_set_status( is_playing );
}

int stop_player( Player* player )
{
	pthread_mutex_lock( &player->the_lock );
	player->control |= PLAYER_CONTROL_EXIT;
	pthread_mutex_unlock( &player->the_lock );

	wake_up_get_buffer_read( &player->circular_buffer );
	assert( !pthread_join( player->audio_thread, NULL ) );

	wake_up_get_buffer_write( &player->circular_buffer );
	assert( !pthread_join( player->reader_thread, NULL ) );

	mpg123_exit();

	// TODO do i need to shutdown anything audio related?

	return 0;
}
