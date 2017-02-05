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

	// TODO ensure that ID_DATA_START messages are smaller than this
	player->max_payload_size = mpg123_outblock( player->mh ) + 1 + sizeof(size_t);

	player->playing_index = 0;

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

int player_change_track_by_id( Player *player, int playlist, int track, int when )
{
	PlaylistItem *item = NULL;
	playlist_manager_get_item( player->playlist_manager, playlist, track, &item );
	LOG_DEBUG("item=p calling player_change_track", item);
	return player_change_track( player, item, when );
}

int player_change_track( Player *player, PlaylistItem *playlist_item, int when )
{
	if( when != TRACK_CHANGE_IMMEDIATE && when != TRACK_CHANGE_NEXT ) {
		return 1;
	}

	LOG_DEBUG("changing player track");
	pthread_mutex_lock( &player->change_track_lock );
	player->change_track = when;
	player->change_playlist_item = playlist_item;
	pthread_mutex_unlock( &player->change_track_lock );

	return 0;
}

int player_notify_item_change( Player *player, PlaylistItem *playlist_item )
{
	// TODO trigger a reload buffer event if required
	return 0;
}

int player_add_metadata_observer( Player *player, MetadataObserver observer, void *data )
{
	printf("player_add_metadata_observer\n");
	if( player->metadata_observers_num == player->metadata_observers_cap ) {
		return 1;
	}
	player->metadata_observers[player->metadata_observers_num] = observer;
	player->metadata_observers_data[player->metadata_observers_num] = data;
	player->metadata_observers_num++;
	return 0;
}

void call_observers( Player *player ) {
	for( int i = 0; i < player->metadata_observers_num; i++ ) {
		player->metadata_observers[i]( player->playing, "todo:playlist_name", &player->current_track, player->metadata_observers_data[i] );
	}
}

bool get_text( Player *player ) {
	//mpg123_id3v1 *v1;
	//mpg123_id3v2 *v2;
	//char *icy_meta;
	//int res;

	//int meta = mpg123_meta_check( player->mh );
	//if( meta & MPG123_NEW_ID3 ) {
	//	if( mpg123_id3( player->mh, &v1, &v2) == MPG123_OK ) {
	//		if( v2 != NULL ) {
	//			//strncpy( player->artist, v2->artist->p, PLAYER_ARTIST_LEN );
	//			//strncpy( player->title, v2->title->p, PLAYER_TITLE_LEN );
	//			return true;
	//		} else if( v1 != NULL ) {
	//			//strncpy( player->artist, v1->artist, PLAYER_ARTIST_LEN );
	//			//strncpy( player->title, v1->title, PLAYER_TITLE_LEN );
	//			return true;
	//		}
	//	}
	//}
	//if( meta & MPG123_NEW_ICY ) {
	//	if( mpg123_icy( player->mh, &icy_meta) == MPG123_OK ) {
	//		printf("got ICY: %s\n", icy_meta);
	//		char *station;
	//		res = parse_icy( icy_meta, &station );
	//		if( res ) {
	//			LOG_ERROR( "icymeta=s failed to decode icy", icy_meta );
	//		} else {
	//			strncpy( player->artist, station, PLAYER_ARTIST_LEN );
	//			strncpy( player->title, "", PLAYER_TITLE_LEN );
	//			free( station );
	//			return true;
	//		}
	//	}
	//}
	return false;
}

void rewind_to_next( Player *player, bool next_song )
{
	size_t decoded_size2;
	char *pp[2];
	size_t size[2];

	LOG_DEBUG( "Searching for next song" );
	pthread_mutex_lock( &player->circular_buffer.lock );

	get_buffer_read_unsafe2( &player->circular_buffer, 0, &pp[0], &size[0], &pp[1], &size[1] );

	size_t min_bytes_to_read = player->audio_thread_size[0] + player->audio_thread_size[1];
	size_t num_read = 0;
	char *found = NULL;
	for( int j = 0; j < 2 && !found; j++ ) {
		while( size[j] > 0 ) {
			char *q = pp[j];

			LOG_DEBUG( "j=d q=p here", j, q );
			unsigned char payload_id = *(unsigned char*) q;
			if( num_read >= min_bytes_to_read ) {
				if( !next_song || payload_id == ID_DATA_START ) {
					found = q;
					break;
				}
			}
			q++;
			num_read++;
			size[j]--;

			if( payload_id == AUDIO_DATA ) {
				decoded_size2 = *((size_t*) q);
				q += sizeof(size_t);
				num_read += sizeof(size_t);
				size[j] -= sizeof(size_t);

				q += decoded_size2;
				num_read += decoded_size2;
				size[j] -= decoded_size2;
			} else {
				assert( 0 ); //unsupported payload_id
			}
			pp[j] = q;
		}
	}

	if( found ) {
		LOG_DEBUG("p=p rewinding buffer", found);
		buffer_rewind_unsafe( &player->circular_buffer, found );
	}

	pthread_mutex_unlock( &player->circular_buffer.lock );
}

void rewind_to_next_message( Player *player )
{
	rewind_to_next( player, false );
}

void rewind_to_next_song( Player *player )
{
	rewind_to_next( player, true );
}

void player_load_into_buffer( Player *player, PlaylistItem *playlist_item )
{
	bool done;
	int res;
	char *p;
	int fd;
	size_t buffer_free;
	size_t *decoded_size;

	bool is_stream = false;
	off_t icy_interval;
	size_t bytes_written;
	
	mpg123_id3v1 *v1;
	mpg123_id3v2 *v2;

	PlayQueueItem *pqi = NULL;

	LOG_DEBUG("path=s opening file in reader", playlist_item->path);
	res = open_fd( playlist_item->path, &fd, &is_stream, &icy_interval );
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
	PlayerTrackInfo *track_info = (PlayerTrackInfo*) p;
	memset( track_info, 0, sizeof(PlayerTrackInfo) );

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

		res = get_buffer_write( &player->circular_buffer, player->max_payload_size, &p, &buffer_free );
		if( res ) {
			//LOG_DEBUG("buffer full");
			sleep(1);
			continue;
		}

		// dont read too much
		buffer_free = player->max_payload_size;

		//LOG_DEBUG("writing AUDIO_DATA header");
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

		res = mpg123_read( player->mh, (unsigned char *)p, buffer_free, decoded_size);
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
		p += *decoded_size;
		if( *decoded_size > 0 ) {
			bytes_written += *decoded_size;
			//LOG_DEBUG("size=d wrote decoded data", bytes_written);
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

//void player_mark_deleted( void *data )
//{
//	// scan through all songs in playqueue
//	//if any of the song->next references the deleted
//	//song, update the reference
//	//then mark the buffer after the song as invalidated, and force a reload
//}

void player_reader_thread_run( void *data )
{
	Player *player = (Player*) data;
	PlaylistItem *playlist_item = NULL;
	int change_track = 0;;
	int res;
	PlayQueueItem *pqi = NULL;

	for(;;) {

		pthread_mutex_lock( &player->change_track_lock );
		if( player->change_track ) {
			player->change_track = 0;
			playlist_item = player->change_playlist_item;
			change_track = player->change_track;
		}
		pthread_mutex_unlock( &player->change_track_lock );

		if( !playlist_item ) {
			usleep(1000);
			continue;
		}

		if( change_track == TRACK_CHANGE_IMMEDIATE ) {
			// TODO
			assert(0);
		} else if( change_track == TRACK_CHANGE_NEXT ) {
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
		}

		player_load_into_buffer( player, playlist_item );

		playlist_manager_lock( player->playlist_manager );
		LOG_DEBUG("playlist_item=p next=p setting next song", playlist_item, playlist_item->next);
		playlist_item = playlist_item->next;
		playlist_manager_unlock( player->playlist_manager );

	}
}
//
//void player_reader_thread_run( void *data )
//{
//	Player *player = (Player*) data;
//	
//	bool done;
//	int res;
//	char *p;
//	const char *path;
//	int fd;
//	size_t buffer_free;
//	size_t *decoded_size;
//
//	bool is_stream = false;
//	off_t icy_interval;
//	size_t bytes_written;
//	
//	mpg123_id3v1 *v1;
//	mpg123_id3v2 *v2;
//
//	PlayQueueItem *pqi = NULL;
//	PlaylistItem *item = NULL;
//
//
//	int change_track;
//
//	for(;;) {
//		change_track = 0;
//		pthread_mutex_lock( &player->change_track_lock );
//		if( player->change_track ) {
//			change_track = player->change_track;
//			player->reading_playlist_id = player->change_playlist_id;
//			player->reading_playlist_track = player->change_playlist_track;
//			player->change_track = 0;
//		}
//		pthread_mutex_unlock( &player->change_track_lock );
//
//		if( change_track == TRACK_CHANGE_IMMEDIATE ) {
//			rewind_to_next_message( player );
//			change_track = 0;
//			//TODO send a signal to the player to skip the current buffer (for now it'll be a short lag)
//			player->next_track = true;
//		} else if( change_track == TRACK_CHANGE_NEXT ) {
//			rewind_to_next_song( player );
//			change_track = 0;
//		}
//
//		// Get song to play
//		playlist_manager_lock( player->playlist_manager );
//
//		playlist_manager
//		res = playlist_manager_get_item( player->playlist_manager, player->reading_playlist_id, player->reading_playlist_track, &playlist_item );
//		if( res ) {
//			LOG_ERROR( "playlist=d track=d unable to get playlist item", player->reading_playlist_id, player->reading_playlist_track );
//			playlist_manager_unlock( player->playlist_manager );
//			sleep(1);
//			continue;
//		}
//
//		playlist_manager_unlock( player->playlist_manager );
//
//		LOG_DEBUG("path=s opening file in reader", playlist_item->path);
//		res = open_fd( playlist_item->path, &fd, &is_stream, &icy_interval );
//		if( res ) {
//			LOG_ERROR( "unable to open" );
//			playlist_manager_unlock( player->playlist_manager );
//			sleep(1);
//			//TODO deref playlist_item
//			continue;
//		}
//
//		mpg123_param( player->mh, MPG123_ICY_INTERVAL, icy_interval, 0);
//
//		if( mpg123_open_fd( player->mh, fd ) != MPG123_OK ) {
//			LOG_ERROR( "mpg123_open_fd failed" );
//			close( fd );
//			//TODO deref playlist_item
//			continue;
//		}
//
//		for(;;) {
//			if( pqi == NULL ) {
//				res = play_queue_add( &player->play_queue, &pqi );
//				if( res ) {
//					LOG_DEBUG( "play queue full" );
//					sleep(1);
//					continue;
//				}
//			}
//
//			res = get_buffer_write( &player->circular_buffer, player->max_payload_size, &p, &buffer_free );
//			if( res ) {
//				//LOG_DEBUG("buffer full");
//				sleep(1);
//				continue;
//			}
//			break
//		}
//
//		pqi->buf_start = p;
//		pqi->playlist_item = playlist_item;
//
//		pqi = NULL;
//
//		playlist_manager_lock( player->playlist_manager );
//		playlist_item = playlist_item->next;
//		playlist_manager_unlock( player->playlist_manager );
//
//		//LOG_DEBUG("p=p writing ID_DATA_START header", p);
//		*((unsigned char*)p) = ID_DATA_START;
//		p++;
//		PlayerTrackInfo *track_info = (PlayerTrackInfo*) p;
//		memset( track_info, 0, sizeof(PlayerTrackInfo) );
//
//		res = mpg123_seek( player->mh, 0, SEEK_SET );
//		if( mpg123_id3( player->mh, &v1, &v2 ) == MPG123_OK ) {
//			//printf("got meta\n");
//			if( v1 != NULL ) {
//				strcpy( track_info->artist, v1->artist );
//				strcpy( track_info->title, v1->title );
//			}
//		}
//
//		buffer_mark_written( &player->circular_buffer, 1 + sizeof(PlayerTrackInfo) );
//
//		
//		done = false;
//		while( !done ) {
//			pthread_mutex_lock( &player->change_track_lock );
//			if( player->change_track == TRACK_CHANGE_IMMEDIATE ) {
//				LOG_DEBUG("quiting read loop due to immediate change");
//				done = true;
//			}
//			pthread_mutex_unlock( &player->change_track_lock );
//			if( done ) {
//				break;
//			}
//
//			res = get_buffer_write( &player->circular_buffer, player->max_payload_size, &p, &buffer_free );
//			if( res ) {
//				//LOG_DEBUG("buffer full");
//				sleep(1);
//				continue;
//			}
//
//			// dont read too much
//			buffer_free = player->max_payload_size;
//
//			//LOG_DEBUG("writing AUDIO_DATA header");
//			*((unsigned char*)p) = AUDIO_DATA;
//			p++;
//			buffer_free--;
//			bytes_written = 1;
//
//			// reserve some space for number of bytes decoded
//			decoded_size = (size_t*) p;
//			p += sizeof(size_t);
//			buffer_free -= sizeof(size_t);
//			bytes_written += sizeof(size_t);
//
//			*decoded_size = 0;
//
//			res = mpg123_read( player->mh, (unsigned char *)p, buffer_free, decoded_size);
//			switch( res ) {
//				case MPG123_OK:
//					break;
//				case MPG123_NEW_FORMAT:
//					LOG_DEBUG("TODO handle new format");
//					break;
//				case MPG123_DONE:
//					done = true;
//					break;
//				default:
//					LOG_ERROR("err=s unhandled mpg123 error", mpg123_plain_strerror(res));
//					break;
//			}
//			if( *decoded_size > 0 ) {
//				bytes_written += *decoded_size;
//				//LOG_DEBUG("size=d wrote decoded data", bytes_written);
//				buffer_mark_written( &player->circular_buffer, bytes_written );
//			}
//		}
//		mpg123_close( player->mh );
//		close( fd );
//	}
//}

void player_audio_thread_run( void *data )
{
	int res;
	Player *player = (Player*) data;

	//play_tone( player );
	//setbuf(stdout, NULL);

	size_t chunk_size;
	char *q;
	unsigned char payload_id;

	size_t num_read = 0;
	size_t num_read_total = 0;

	char *p[2] = {NULL, NULL};
	size_t size[2] = {0, 0};

	// TODO figure out how much to reserve here
	size_t max_size = player->max_payload_size * 10;

	PlayQueueItem *pqi = NULL;
	//PlaylistItem *playlist_item = NULL;

	for(;;) {
		pthread_mutex_lock( &player->play_queue_lock );
		res = play_queue_head( &player->play_queue, &pqi );
		if( !res ) {
			//playlist_item = pqi->playlist_item;
			play_queue_pop( &player->play_queue );
			pqi = NULL;
		}
		pthread_mutex_unlock( &player->play_queue_lock );
		if( res ) {
			// no track available
			usleep(1000);
			continue;
		}

		for(;;) {
			//LOG_DEBUG("num_read=d num_read_total=d size[0]=d audio loop", num_read, num_read_total, size[0]);
			num_read_total += num_read;
			res = buffer_timedlock( &player->circular_buffer );
			if( !res ) {
				//lock acquired
				if( num_read_total ) {
					buffer_mark_read_unsafe( &player->circular_buffer, num_read_total );
					//LOG_DEBUG("read=d incrased read", player->circular_buffer.read);
					num_read_total = 0;
				}
				get_buffer_read_unsafe2( &player->circular_buffer, 0, &p[0], &size[0], &p[1], &size[1] ); //TODO remove maxsize arg
				//LOG_DEBUG( "p1=p size1=d p2=p size2=d get_buffer_read_unsafe2", p[0], size[0], p[1], size[1] );

				// dont consume too much at a time
				if( size[0] > max_size ) {
					size[0] = max_size;
				}
				if( size[1] > (max_size - size[0] ) ) {
					size[1] = max_size - size[0];
				}

				player->audio_thread_p[0] = p[0];
				player->audio_thread_size[0] = size[0];
				player->audio_thread_p[1] = p[1];
				player->audio_thread_size[1] = size[1];

				buffer_unlock( &player->circular_buffer );
				num_read = 0;
			} else {
				//LOG_DEBUG("failed to acquire lock");
				// unable to acquire lock
				assert( num_read <= size[0] );
				size[0] -= num_read;
				p[0] = p[0] + num_read;
				if( size[0] == 0 ) {
					p[0] = p[1];
					size[0] = size[1];
					p[1] = NULL;
					size[1] = 0;
					//LOG_DEBUG("moving to pointer 2");
				}
				num_read = 0;
			}

			//LOG_DEBUG("p=p reading data", p[0]);

			if( size[0] == 0 ) {
				//LOG_DEBUG("buffer underrun");
				continue;
			}

			q = p[0];

			payload_id = *(unsigned char*) q;
			q++;
			num_read++;

			if( payload_id == ID_DATA_END ) {
				LOG_DEBUG( " ----- trigger end of song ----- " );
				break;
			}


			if( payload_id == ID_DATA_START ) {
				LOG_DEBUG( " ------------ reading ID_DATA_START ------------ " );
				player->next_track = false;
				memcpy( &player->current_track, q, sizeof(PlayerTrackInfo) );
				LOG_DEBUG( "artist=s title=s playing new track", player->current_track.artist, player->current_track.title );
			//	call_observers( player );
				num_read += sizeof(PlayerTrackInfo);
				continue;
			}
			
			//LOG_DEBUG( "reading header" );

			// otherwise it must be audio data
			assert( payload_id == AUDIO_DATA );

			size_t decoded_size = *((size_t*) q);
			q += sizeof(size_t);
			num_read += sizeof(size_t);

			//LOG_DEBUG( "q=p decode_size=d reading data", q, decoded_size );

			chunk_size = 1024;
			while( decoded_size > 0 ) {
				if( decoded_size < chunk_size ) {
					chunk_size = decoded_size;
				}
				if( !player->next_track ) {
					ao_play( player->dev, q, chunk_size );
				}
				q += chunk_size;
				decoded_size -= chunk_size;
				num_read += chunk_size;
			}
		}
	}
}

int stop_player( Player *player )
{
	mpg123_exit();
	ao_shutdown();
	return 0;
}
