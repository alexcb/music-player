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

int init_player( Player *player )
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

	player->decode_buffer_size = mpg123_outblock( player->mh );
	player->decode_buffer = malloc(player->decode_buffer_size);

	// TODO ensure that ID_DATA_START messages are smaller than this
	player->max_payload_size = mpg123_outblock( player->mh ) + 1 + sizeof(size_t);

	pthread_mutex_init( &player->change_track_lock, NULL );
	player->change_track = 0;
	player->change_playlist_item = NULL;

	player->reading_playlist_id = -1;
	player->reading_playlist_track = -1;

	player->audio_thread_size[0] = 0;
	player->audio_thread_size[1] = 0;
	player->audio_thread_p[0] = NULL;
	player->audio_thread_p[1] = NULL;

	player->next_track = false;

	res = init_circular_buffer( &player->circular_buffer, 10*1024*1024 );
	if( res ) {
		goto error;
	}

	play_queue_init( &player->play_queue );
	pthread_mutex_init( &player->play_queue_lock, NULL );

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

int player_change_track( Player *player, PlaylistItem *playlist_item, int when )
{
	if( when != TRACK_CHANGE_IMMEDIATE && when != TRACK_CHANGE_NEXT ) {
		return 1;
	}

	LOG_DEBUG("changing player track");
	pthread_mutex_lock( &player->change_track_lock );
	player->change_track = when;
	if( player->change_playlist_item ) {
		playlist_item_ref_down( player->change_playlist_item );
	}
	player->change_playlist_item = playlist_item;
	playlist_item_ref_up( player->change_playlist_item );
	pthread_mutex_unlock( &player->change_track_lock );

	return 0;
}

int player_reload_next_track( Player *player)
{
	LOG_DEBUG("reloading next track data");
	if( player->current_track.playlist_item == NULL ) {
		return 0;
	}
	PlaylistItem *item = player->current_track.playlist_item->next;
	if( item == NULL ) {
		item = player->current_track.playlist_item->parent->root;
		if( item == NULL ) {
			return 1;
		}
	}
	player_change_track( player, item, TRACK_CHANGE_NEXT );
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
		player->metadata_observers[i]( player->playing, &player->current_track, player->metadata_observers_data[i] );
	}
	LOG_DEBUG("done call observers");
}

void rewind2( Player *player )
{
	size_t decoded_size;
	char *pp[2];
	size_t size[2];

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
					num_read += sizeof(PlayerTrackInfo);
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

	pthread_mutex_unlock( &player->circular_buffer.lock );
}

void player_load_into_buffer( Player *player, PlaylistItem *playlist_item )
{
	bool done;
	int res;
	char *p;
	int fd;
	size_t buffer_free;
	size_t decoded_size;

	bool is_stream = false;
	off_t icy_interval;
	char *icy_meta;
	char *icy_title;
	char *icy_name = NULL;
	size_t bytes_written;
	PlayerTrackInfo *track_info;
	
	mpg123_id3v1 *v1;
	mpg123_id3v2 *v2;

	PlayQueueItem *pqi = NULL;

	// dont start loading a stream if paused
	if( strstr(playlist_item->track->path, "http://") ) {
		while( !player->playing ) {
			usleep(100);
		}
	}

	LOG_DEBUG("path=s opening file in reader", playlist_item->track->path);
	res = open_fd( playlist_item->track->path, &fd, &is_stream, &icy_interval, &icy_name );
	if( res ) {
		LOG_ERROR( "unable to open" );
		playlist_manager_unlock( player->playlist_manager );
		return;
	}

	mpg123_param( player->mh, MPG123_ICY_INTERVAL, icy_interval, 0);

	if( mpg123_open_fd( player->mh, fd ) != MPG123_OK ) {
		LOG_ERROR( "mpg123_open_fd failed" );
		close( fd );
		return;
	}

	for(;;) {
		res = get_buffer_write( &player->circular_buffer, player->max_payload_size, &p, &buffer_free );
		if( res ) {
			//LOG_DEBUG("buffer full");
			sleep(1);
			continue;
		}
		break;
	}

	for(;;) {
		pthread_mutex_lock( &player->play_queue_lock );
		res = play_queue_add( &player->play_queue, &pqi );
		pthread_mutex_unlock( &player->play_queue_lock );
		if( res ) {
			LOG_DEBUG( "play queue full" );
			sleep(1);
			continue;
		}
		break;
	}

	pqi->buf_start = p;
	pqi->playlist_item = playlist_item;
	pqi = NULL;

	//LOG_DEBUG("p=p writing ID_DATA_START header", p);
	*((unsigned char*)p) = ID_DATA_START;
	p++;
	track_info = (PlayerTrackInfo*) p;
	memset( track_info, 0, sizeof(PlayerTrackInfo) );
	track_info->playlist_item = playlist_item;
	track_info->is_stream = is_stream;
	if( icy_name ) {
		strcpy( track_info->icy_name, icy_name );
	}

	res = mpg123_seek( player->mh, 0, SEEK_SET );
	if( mpg123_id3( player->mh, &v1, &v2 ) == MPG123_OK ) {
		if( v1 != NULL ) {
			strcpy( track_info->artist, v1->artist );
			strcpy( track_info->title, v1->title );
		}
	}

	buffer_mark_written( &player->circular_buffer, 1 + sizeof(PlayerTrackInfo) );

	
	done = false;
	while( !done ) {
		pthread_mutex_lock( &player->change_track_lock );
		if( player->change_track == TRACK_CHANGE_IMMEDIATE ) {
			LOG_DEBUG("quiting read loop due to immediate change");
			done = true;
		}
		pthread_mutex_unlock( &player->change_track_lock );
		if( done ) {
			break;
		}

		if( !player->playing && is_stream ) {
			// can't pause a stream; just quit
			LOG_DEBUG("quit buffering due to pause of stream");
			break;
		}

		// TODO ensure there is enough room for audio + potential ICY data; FIXME for now just multiply by 2
		res = get_buffer_write( &player->circular_buffer, player->max_payload_size * 2, &p, &buffer_free );
		if( res ) {
			//LOG_DEBUG("buffer full");
			sleep(1);
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
					track_info = (PlayerTrackInfo*) p;
					p += sizeof(PlayerTrackInfo);

					bytes_written += 1 + sizeof(PlayerTrackInfo);

					memset( track_info, 0, sizeof(PlayerTrackInfo) );
					track_info->playlist_item = playlist_item;
					track_info->is_stream = is_stream;
					strcpy( track_info->artist, "" );
					strcpy( track_info->title, icy_title );
					strcpy( track_info->icy_name, icy_name );
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
	mpg123_close( player->mh );
	close( fd );

	LOG_DEBUG("writting end data msg");
	*((unsigned char*)p) = ID_DATA_END;
	p++;
	buffer_mark_written( &player->circular_buffer, 1 );
}

void player_reader_thread_run( void *data )
{
	Player *player = (Player*) data;
	PlaylistItem *playlist_item = NULL;
	int change_track = 0;
	int res;
	PlayQueueItem *pqi = NULL;

	for(;;) {
		pthread_mutex_lock( &player->change_track_lock );
		if( player->change_track ) {
			change_track = player->change_track;
			playlist_item = player->change_playlist_item;
			player->change_track = 0;
		}
		pthread_mutex_unlock( &player->change_track_lock );

		if( !playlist_item ) {
			usleep(1000);
			continue;
		}

		if( change_track == TRACK_CHANGE_IMMEDIATE ) {
			LOG_DEBUG("handling TRACK_CHANGE_IMMEDIATE");
			pthread_mutex_lock( &player->play_queue_lock );
			play_queue_clear( &player->play_queue );
			rewind2( player );
			player->next_track = true;
			pthread_mutex_unlock( &player->play_queue_lock );
			change_track = 0;
		} else if( change_track == TRACK_CHANGE_NEXT ) {
			LOG_DEBUG("handling TRACK_CHANGE_NEXT");
			pthread_mutex_lock( &player->play_queue_lock );
			res = play_queue_head( &player->play_queue, &pqi );
			if( !res ) {
				LOG_DEBUG("rewinding");
				buffer_lock( &player->circular_buffer );
				buffer_rewind_unsafe( &player->circular_buffer, pqi->buf_start );
				buffer_unlock( &player->circular_buffer );
			}
			play_queue_clear( &player->play_queue );
			pthread_mutex_unlock( &player->play_queue_lock );
			change_track = 0;
		}

		player_load_into_buffer( player, playlist_item );

		playlist_manager_lock( player->playlist_manager );
		LOG_DEBUG("playlist_item=p next=p setting next song", playlist_item, playlist_item->next);
		if( playlist_item->next ) {
			playlist_item = playlist_item->next;
		} else {
			playlist_item = playlist_item->parent->root;
		}
		playlist_manager_unlock( player->playlist_manager );
	}
}

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
		pthread_mutex_lock( &player->play_queue_lock );
		res = play_queue_head( &player->play_queue, &pqi );
		if( !res ) {
			play_queue_pop( &player->play_queue );
			pqi = NULL;
		}
		pthread_mutex_unlock( &player->play_queue_lock );
		if( res ) {
			//TODO call observers saying we're not playing anything. but only the first time.
			// no track available
			if( !notified_no_songs ) {
				memset( &player->current_track, 0, sizeof(PlayerTrackInfo) );
				call_observers( player );
				notified_no_songs = true;
			}
			usleep(1000);
			continue;
		}
		notified_no_songs = false;
		last_play_state = !player->playing;

		for(;;) {
			num_read = 0;
			res = get_buffer_read( &player->circular_buffer, &p, &buffer_avail );
			if( res ) {
				usleep(1000);
				continue;
			}

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
				memcpy( &player->current_track, p, sizeof(PlayerTrackInfo) );
				player->current_track.playlist_item->parent->current = player->current_track.playlist_item;
				LOG_DEBUG( "artist=s title=s playing new track", player->current_track.artist, player->current_track.title );
				call_observers( player );
				num_read += sizeof(PlayerTrackInfo);
				buffer_mark_read( &player->circular_buffer, num_read );
				continue;
			}

			// otherwise it must be audio data
			assert( payload_id == AUDIO_DATA );

			size_t decoded_size = *((size_t*) p);
			p += sizeof(size_t);
			num_read += sizeof(size_t);

			chunk_size = 1024;
			while( decoded_size > 0 ) {
				if( last_play_state != player->playing ) {
					LOG_DEBUG("calling obs here");
					call_observers( player );
					last_play_state = player->playing;
				}

				if( decoded_size < chunk_size ) {
					chunk_size = decoded_size;
				}

				if( !player->next_track ) {
					bool should_play = true;
					if( !player->playing ) {
						if( !player->current_track.is_stream) {
							usleep(100);
							continue;
						}
						should_play = false;
					}

					if( should_play ) {
						ao_play( player->dev, p, chunk_size );
					}
				}
				p += chunk_size;
				decoded_size -= chunk_size;
				num_read += chunk_size;
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
