#include "player.h"
#include "icy.h"
#include "log.h"
#include "io_utils.h"
#include "albums.h"

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
#define ID_DATA_START 2
#define ID_DATA_END 3

#define BITS 8

void player_audio_thread_run( void *data );
void player_reader_thread_run( void *data );

int init_player( Player *player, const char *library_path )
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
	mpg123_format_none( player->mh );
	mpg123_format( player->mh, 44100, MPG123_STEREO, MPG123_ENC_SIGNED_16 );

	player->decode_buffer_size = mpg123_outblock( player->mh );
	player->decode_buffer = malloc(player->decode_buffer_size);

	// TODO ensure that ID_DATA_START messages are smaller than this
	player->max_payload_size = mpg123_outblock( player->mh ) + 1 + sizeof(size_t);

	pthread_mutex_init( &player->the_lock, NULL );

	player->next_track = false;
	player->load_in_progress = false;
	player->load_abort_requested = false;

	player->playing = false;
	player->current_track = NULL;

	player->playlist_item_to_buffer = NULL;
	player->playlist_item_to_buffer_override = NULL;

	player->library_path = library_path;

	res = pthread_cond_init( &(player->load_cond), NULL );
	assert( res == 0 );

	res = init_circular_buffer( &player->circular_buffer, 10*1024*1024 );
	if( res ) {
		goto error;
	}

	play_queue_init( &player->play_queue );

	player->metadata_observers_num = 0;
	player->metadata_observers_cap = 2;
	player->metadata_observers = (MetadataObserver*) malloc(sizeof(MetadataObserver) * player->metadata_observers_cap);
	if( player->metadata_observers == NULL ) {
		goto error;
	}
	player->metadata_observers_data = (void**) malloc(sizeof(void*) * player->metadata_observers_cap);
	if( player->metadata_observers_data == NULL ) {
		goto error;
	}
	return 0;

error:
	//free( player->buffer );
	//mpg123_exit();
	//ao_shutdown();
	return 1;
}

int start_player( Player *player )
{
	int res;
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
	return 1;
}

void stop_loader( Player *player )
{
	int res;
	while( player->load_in_progress ) {
		LOG_DEBUG("waiting for track loader to respond to abort request");
		player->load_abort_requested = true;
		res = pthread_cond_wait( &player->load_cond, &player->the_lock );
		assert( res == 0 );
	}
	LOG_DEBUG("track loader is stopped");
}

int player_change_next_album( Player *player, int when )
{
	int res = 1;
	PlaylistItem *p;

	pthread_mutex_lock( &player->the_lock );

	if( player->current_track != NULL ) {
		for( p = player->current_track; p != NULL; p = p->next ) {
			if( strcmp(player->current_track->track->album, p->track->album) != 0 ) {
				break;
			}
		}
		if( p == NULL ) {
			// start back at the begining
			p = player->current_track->parent->root;
		}

		res = player_change_track( player, p, when )
	}

	pthread_mutex_unlock( &player->the_lock );
	return res;
}

int player_change_track( Player *player, PlaylistItem *playlist_item, int when )
{
	int res;
	if( when != TRACK_CHANGE_IMMEDIATE && when != TRACK_CHANGE_NEXT ) {
		return 1;
	}

	LOG_DEBUG("player_change_track -- locking");
	pthread_mutex_lock( &player->the_lock );

	res = player_change_track_unsafe( player, playlist_item, when );

	LOG_DEBUG("player_change_track -- unlocking");
	pthread_mutex_unlock( &player->the_lock );

	return res;
}

int player_change_track_unsafe( Player *player, PlaylistItem *playlist_item, int when )
{
	LOG_DEBUG("player_change_track -- set next_track=true");
	player->next_track = true;

	if( when == TRACK_CHANGE_IMMEDIATE ) {
		stop_loader( player );
	}

	LOG_DEBUG("player_change_track -- rewinding");
	player_rewind_buffer_unsafe( player );
	player->playlist_item_to_buffer_override = playlist_item;

	return 0;
}

int player_notify_item_change( Player *player, PlaylistItem *playlist_item )
{
	// TODO trigger a reload buffer event if required
	return 0;
}

int player_add_metadata_observer( Player *player, MetadataObserver observer, void *data )
{
	LOG_DEBUG("player_add_metadata_observer");
	if( player->metadata_observers_num == player->metadata_observers_cap ) {
		return 1;
	}
	player->metadata_observers[player->metadata_observers_num] = observer;
	player->metadata_observers_data[player->metadata_observers_num] = data;
	player->metadata_observers_num++;
	return 0;
}

void call_observers( Player *player ) {
	LOG_DEBUG("start call observers");
	for( int i = 0; i < player->metadata_observers_num; i++ ) {
		player->metadata_observers[i]( player->playing, player->current_track, player->metadata_observers_data[i] );
	}
	LOG_DEBUG("done call observers");
}

void rewind2( Player *player )
{
	size_t decoded_size;
	char *pp[2];
	size_t size[2];

	LOG_DEBUG("locking - rewind2");
	pthread_mutex_lock( &player->circular_buffer.lock );

	get_buffer_read_unsafe2( &player->circular_buffer, 0, &pp[0], &size[0], &pp[1], &size[1] );

	size_t num_read;
	int num_messages = 0;
	char *found = NULL;
	for( int j = 0; j < 2 && !found; j++ ) {
		LOG_DEBUG( "j=d size=d here", j, size[j] );
		while( size[j] > 0 ) {
			char *q = pp[j];
			unsigned char payload_id = *(unsigned char*) q;

			if( ++num_messages == 2 ) {
				found = q;
				break;
			}

			q++;
			num_read = 1;

			switch( payload_id ) {
				case ID_DATA_START:
					break;
				case ID_DATA_END:
					break;
				case AUDIO_DATA:
					decoded_size = *((size_t*) q);
					num_read += sizeof(size_t) + decoded_size;
					break;
				default:
					assert( 0 ); //unsupported payload_id
			}

			size[j] -= num_read;
			pp[j] += num_read;
		}
	}

	if( found ) {
		LOG_DEBUG("p=p rewinding buffer", found);
		buffer_rewind_unsafe( &player->circular_buffer, found );
	}

	LOG_DEBUG("unlocking - rewind2");
	pthread_mutex_unlock( &player->circular_buffer.lock );
}

bool player_should_abort_load( Player *player )
{
	bool b = false;
	//LOG_DEBUG("locking - player_should_abort_load");
	pthread_mutex_lock( &player->the_lock );
	b = player->load_abort_requested;
	//LOG_DEBUG("unlocking - player_should_abort_load");
	pthread_mutex_unlock( &player->the_lock );
	return b;
}

void player_load_into_buffer( Player *player, PlaylistItem *item )
{
	bool done;
	int res;
	char *p;
	int fd = 0;
	size_t buffer_free;
	size_t decoded_size;

	bool is_stream = false;
	off_t icy_interval;
	char *icy_meta;
	char *icy_title;
	char *icy_name = NULL;
	size_t bytes_written;
	
	PlayQueueItem *pqi = NULL;
	const char *path = item->track->path;

	// dont start loading a stream if paused
	if( strstr(path, "http://") ) {
		while( !player->playing ) {
			usleep(100);
			if( player_should_abort_load( player )) { goto done; }
		}
	}

	sds full_path = sdscatfmt( NULL, "%s/%s", player->library_path, path);

	LOG_DEBUG("path=s opening file in reader", full_path);
	//sleep(2); // simulate a sleep
	res = open_fd( full_path, &fd, &is_stream, &icy_interval, &icy_name );
	if( res ) {
		LOG_ERROR( "unable to open" );
		goto done;
	}
	LOG_DEBUG("path=s opened file in reader", full_path);

	mpg123_param( player->mh, MPG123_ICY_INTERVAL, icy_interval, 0);

	if( mpg123_open_fd( player->mh, fd ) != MPG123_OK ) {
		LOG_ERROR( "mpg123_open_fd failed" );
		goto done;
	}

	// acquire an empty play queue item (busy-wait until one is free)
	for(;;) {
		LOG_DEBUG("locking - player_load_into_buffer play queue add");
		pthread_mutex_lock( &player->the_lock );

		res = get_buffer_write( &player->circular_buffer, player->max_payload_size, &p, &buffer_free );
		if( res ) {
			pthread_mutex_unlock( &player->the_lock );
			LOG_DEBUG("write buffer is full");

			if( player_should_abort_load( player )) { goto done; }
			usleep(50000);
			continue;
		}

		res = play_queue_add( &player->play_queue, &pqi );
		if( res ) {
			pthread_mutex_unlock( &player->the_lock );

			if( player_should_abort_load( player )) { goto done; }
			usleep(50000);
			continue;
		}
		
		LOG_DEBUG("p=p path=s adding pqi", p, item->track->path);
		pqi->buf_start = p;
		pqi->playlist_item = item;
		playlist_item_ref_up( item );
		pqi = NULL;

		LOG_DEBUG("unlocking - player_load_into_buffer play queue add");
		pthread_mutex_unlock( &player->the_lock );
		break;
	}

	LOG_DEBUG("p=p starting to write", p);

	//LOG_DEBUG("p=p writing ID_DATA_START header", p);
	*((unsigned char*)p) = ID_DATA_START;
	p++;
	if( icy_name ) {
		LOG_DEBUG("icy=s TODO something with icy name", icy_name);
	}

	buffer_mark_written( &player->circular_buffer, 1 );
	
	done = false;
	while( !done ) {
		if( player_should_abort_load( player )) { goto done; }

		if( !player->playing && is_stream ) {
			// can't pause a stream; just quit
			LOG_DEBUG("quit buffering due to pause of stream");
			break;
		}

		// TODO ensure there is enough room for audio + potential ICY data; FIXME for now just multiply by 2
		res = get_buffer_write( &player->circular_buffer, player->max_payload_size * 2, &p, &buffer_free );
		if( res ) {
			usleep(100);
			continue;
		}

		bytes_written = 0;
		res = mpg123_read( player->mh, (unsigned char *)player->decode_buffer, player->decode_buffer_size, &decoded_size);
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
		int meta = mpg123_meta_check( player->mh );
		if( meta & MPG123_NEW_ICY ) {
			if( mpg123_icy( player->mh, &icy_meta) == MPG123_OK ) {
				res = parse_icy( icy_meta, &icy_title );
				if( res ) {
					LOG_ERROR( "icymeta=s failed to decode icy", icy_meta );
				} else {
					*((unsigned char*)p) = ID_DATA_START;
					p++;

					LOG_DEBUG("icymeta=s TODO something with icy title", icy_title );

					bytes_written += 1;

					//memset( track_info, 0, sizeof(PlayerTrackInfo) );
					//track_info->playlist_item = NULL;
					//track_info->is_stream = is_stream;
					//strcpy( track_info->artist, "" );
					//strcpy( track_info->title, icy_title );
					//strcpy( track_info->icy_name, icy_name );
					free( icy_title );
				}
			}
		}

		if( decoded_size > 0 ) {
			//LOG_DEBUG("writing AUDIO_DATA header");
			*((unsigned char*)p) = AUDIO_DATA;
			p++;
			bytes_written += 1;
			buffer_free--;

			// reserve some space for number of bytes decoded
			*((size_t*)p) = decoded_size;
			p += sizeof(size_t);
			buffer_free -= sizeof(size_t);
			bytes_written += sizeof(size_t);

			memcpy( p, player->decode_buffer, decoded_size );
			p += decoded_size;
			bytes_written += decoded_size;
		}

		if( bytes_written > 0 ) {
			buffer_mark_written( &player->circular_buffer, bytes_written );
		}
	}

	LOG_DEBUG("writting end data msg");
	*((unsigned char*)p) = ID_DATA_END;
	p++;
	buffer_mark_written( &player->circular_buffer, 1 );

done:
	// no easy way to see if is open, but closing a closed reader is fine.
	mpg123_close( player->mh );
	if( fd ) {
		close( fd );
	}
	LOG_DEBUG("path=s returning from player_load_into_buffer", full_path);
	sdsfree( full_path );
}

// caller must first lock the player before calling this
void player_rewind_buffer_unsafe( Player *player )
{
	int res;
	PlayQueueItem *pqi = NULL;

	LOG_DEBUG("handling player_rewind_buffer_unsafe");

	res = play_queue_head( &player->play_queue, &pqi );
	if( !res ) {
		stop_loader( player );
		LOG_DEBUG("p=p rewinding", pqi->buf_start);
		buffer_lock( &player->circular_buffer );
		buffer_rewind_unsafe( &player->circular_buffer, pqi->buf_start );
		buffer_unlock( &player->circular_buffer );
	} else {
		LOG_DEBUG("nothing to rewind");
	}
	LOG_DEBUG("clearing play queue");
	play_queue_clear( &player->play_queue );
}

void player_reader_thread_run( void *data )
{
	Player *player = (Player*) data;
	PlaylistItem *item = NULL;

	for(;;) {
		//LOG_DEBUG("locking - player_reader_thread_run");
		pthread_mutex_lock( &player->the_lock );

		if( player->playlist_item_to_buffer_override != NULL ) {
			player->playlist_item_to_buffer = player->playlist_item_to_buffer_override;
			player->playlist_item_to_buffer_override = NULL;
		}

		if( player->playlist_item_to_buffer != NULL ) {
			item = player->playlist_item_to_buffer;
			playlist_item_ref_up( item );
			player->load_in_progress = true;
			LOG_DEBUG("setting load_in_progress");
			player->load_abort_requested = false;
		} else {
			item = NULL;
		}

		//LOG_DEBUG("unlocking - player_reader_thread_run");
		pthread_mutex_unlock( &player->the_lock );

		if( item == NULL ) {
			usleep(1000);
			continue;
		}

		player_load_into_buffer( player, item );

		LOG_DEBUG("locking - player_reader_thread_run 2");
		pthread_mutex_lock( &player->the_lock );

		playlist_item_ref_down( item );
		item = NULL;

		if( player->playlist_item_to_buffer != NULL ) {
			LOG_DEBUG("playlist_item=p next=p parent=p current=s setting next song", player->playlist_item_to_buffer, player->playlist_item_to_buffer->next, player->playlist_item_to_buffer->parent, player->playlist_item_to_buffer->track->path);
			if( player->playlist_item_to_buffer->next ) {
				player->playlist_item_to_buffer = player->playlist_item_to_buffer->next;
			} else if( player->playlist_item_to_buffer->parent == NULL ) {
				player->playlist_item_to_buffer = NULL;
			} else {
				player->playlist_item_to_buffer = player->playlist_item_to_buffer->parent->root;
			}
		}

		LOG_DEBUG("clearing load_in_progress");
		player->load_in_progress = false;
		pthread_cond_signal( &player->load_cond );

		LOG_DEBUG("unlocking - player_reader_thread_run 2");
		pthread_mutex_unlock( &player->the_lock );
		usleep(100); // need to sleep before re-attempting to aquire the lock, otherwise we can get stuck while changing the song
	}
}

void player_lock( Player *player ) { pthread_mutex_lock( &player->the_lock ); }
void player_unlock( Player *player ) { pthread_mutex_unlock( &player->the_lock ); }

void player_audio_thread_run( void *data )
{
	int res;
	Player *player = (Player*) data;

	size_t chunk_size;
	unsigned char payload_id;

	size_t num_read;

	PlayQueueItem *pqi = NULL;
	//PlaylistItem *playlist_item = NULL;
	
	size_t buffer_avail;
	char *p;
	bool notified_no_songs = false;
	bool last_play_state;
	
	for(;;) {
		sleep(1); // this is causing a bug!
		//LOG_DEBUG("locking - player_audio_thread_run");
		pthread_mutex_lock( &player->the_lock );

		if( player->next_track ) {
			res = play_queue_head( &player->play_queue, &pqi );
			if( res == 0 ) {
				LOG_DEBUG("p=p next=s marking upto buffer", pqi->buf_start, pqi->playlist_item->track->path);
				buffer_mark_read_upto( &player->circular_buffer, pqi->buf_start );
			} else {
				LOG_DEBUG("clearing buffer");
				buffer_clear( &player->circular_buffer );
			}
			player->next_track = false;
		}

		if( player->current_track ) {
			playlist_item_ref_down(player->current_track);
			player->current_track = NULL;
		}

		res = play_queue_head( &player->play_queue, &pqi );
		if( res ) {
			pthread_mutex_unlock( &player->the_lock );

			if( !notified_no_songs ) {
				call_observers( player );
				notified_no_songs = true;
			}
			usleep(50000); //50ms
			continue;
		}

		LOG_DEBUG( "p=p path=s popped play queue item", pqi->buf_start, pqi->playlist_item->track->path );
		player->current_track = pqi->playlist_item;
		pqi = NULL; //once the play_queue is unlocked, this memory will point to something else, make sure we dont use it.
		play_queue_pop( &player->play_queue );

		//LOG_DEBUG("unlocking - player_audio_thread_run 2");
		pthread_mutex_unlock( &player->the_lock );

		notified_no_songs = false;
		last_play_state = !player->playing;

		bool out_of_buffer_logged = false;
		for(;;) {
			num_read = 0;
			res = get_buffer_read( &player->circular_buffer, &p, &buffer_avail );
			if( res ) {
				if( out_of_buffer_logged == false ) {
					LOG_DEBUG( "out of buffer" );
					out_of_buffer_logged = true;
				}
				usleep(50000); // 50ms
				continue;
			}
			out_of_buffer_logged = false;

			payload_id = *(unsigned char*) p;
			p++;
			num_read++;

			if( payload_id == ID_DATA_END ) {
				LOG_DEBUG( " ----- trigger end of song ----- " );
				buffer_mark_read( &player->circular_buffer, num_read );
				break;
			}


			if( payload_id == ID_DATA_START ) {
				LOG_DEBUG( " ------------ reading ID_DATA_START ------------ " );
				player->next_track = false;
				//memcpy( &player->current_track, p, sizeof(PlayerTrackInfo) );
				//player->current_track.playlist_item->parent->current = player->current_track.playlist_item;
				//LOG_DEBUG( "artist=s title=s playing new track", player->current_track.artist, player->current_track.title );
				call_observers( player );
				buffer_mark_read( &player->circular_buffer, num_read );
				continue;
			}

			// otherwise it must be audio data
			if( payload_id != AUDIO_DATA ) {
				LOG_ERROR("p=p bad data", p);
			}

			assert( payload_id == AUDIO_DATA );

			size_t decoded_size = *((size_t*) p);
			p += sizeof(size_t);
			num_read += sizeof(size_t);

			chunk_size = 1024;
			while( decoded_size > 0 && !player->next_track ) {
				if( last_play_state != player->playing ) {
					LOG_DEBUG("calling obs here");
					call_observers( player );
					last_play_state = player->playing;
				}

				if( decoded_size < chunk_size ) {
					chunk_size = decoded_size;
				}

				if( !player->playing ) {
					if( strstr(player->current_track->track->path, "http://") == NULL ) {
						// regular file, busy-wait until we can play again
						usleep(100);
						continue;
					}
				} else {
					ao_play( player->dev, p, chunk_size );
					// uncomment for proof that p gets copied by ao_play: memset( p, 0, chunk_size );
				}

				p += chunk_size;
				decoded_size -= chunk_size;
				num_read += chunk_size;
			}

			if( player->next_track ) {
				LOG_DEBUG("breaking due to next_track");
				break;
			}
			buffer_mark_read( &player->circular_buffer, num_read );
		}
	}
}

void player_set_playing( Player *player, bool playing)
{
	player->playing = playing;
}

int stop_player( Player *player )
{
	mpg123_exit();
	ao_shutdown();
	return 0;
}
