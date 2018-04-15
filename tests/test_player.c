#include "tests.h"

#include "testRunner.h"

#include "player.h"
#include "log.h"

void* player_reader_thread_run( void *data );
void* player_audio_thread_run( void *data );

char *expected_buffer;
size_t expected_buffer_size;
size_t expected_buffer_read;
size_t expected_buffer_len;

int test_player_consumer(Player *player, const char *p, size_t n)
{
	for( int i = 0; i < n; i++ ) {
		char want = expected_buffer[expected_buffer_read++];
		if( p[i] != want ) {
			printf( "got=%x want=%x\n", p[i], want );
			assert(0);
		}
	}
	usleep(10000);

	return 0;
}

volatile unsigned char last_track_seen;
volatile int num_skips;

int test_player_consumer_skip(Player *player, const char *p, size_t n)
{
	for( int i = 0; i < n; i++ ) {
		unsigned char current_track = p[i];
		if( current_track == last_track_seen || current_track == (last_track_seen+1) ) {
			if( last_track_seen != current_track ) {
				printf( "got data for new track: %x\n", current_track );
				last_track_seen = current_track;
				num_skips++;
				usleep(1000000);
			}
		} else if( last_track_seen == 10 && current_track == 1 ) {
			last_track_seen = current_track;
			num_skips++;
			printf( "looped back to start; got data for new track: %x\n", current_track );
			usleep(1000000);
		} else {
			printf( "got=%x want=%x or %x\n", current_track, last_track_seen, last_track_seen+1 );
			assert( 0 );
		}

		//printf( "got=%x\n", p[i] );
		//char want = expected_buffer[expected_buffer_read++];
		//if( p[i] != want ) {
		//	printf( "got=%x want=%x\n", p[i], want );
		//	//assert(0);
		//}
	}

	return 0;
}

void set_buffer( char *p, int seed, size_t n )
{
	srand( seed );
	for( int i = 0; i < n; i++ ) {
		p[i] = (char) rand();
	}
}

void set_buffer_track( char *p, unsigned char track, size_t n )
{
	for( int i = 0; i < n; i++ ) {
		p[i] = (char) track;
	}
}

void status_changed( bool playing, const PlaylistItem *item, void *d )
{
	if( !playing ) {
		printf("paused\n");
		return;
	}

	if( !item ) {
		printf("waiting for data\n");
		return;
	}

	printf("------------ here %s \n", item->track->title );
}

Player* setupTestPlayer()
{
	int res;

	Player *player = (Player*) malloc(sizeof(Player));
	player->pa_handle = NULL,
	player->alsa_handle = NULL,
	player->next_track = NULL,

	player->next_track = NULL,
	player->load_in_progress = false,
	player->load_abort_requested = false,

	player->playing = true,
	player->current_track = NULL,

	player->metadata_observers_num = 0,

	player->playlist_item_to_buffer = NULL,
	player->playlist_item_to_buffer_override = NULL,
	player->audio_consumer = test_player_consumer,

	player->metadata_observers_num = 0;
	player->metadata_observers_cap = 2;
	player->metadata_observers = (MetadataObserver*) malloc(sizeof(MetadataObserver) * player->metadata_observers_cap);
	player->metadata_observers_data = (void*) malloc(sizeof(void*) * player->metadata_observers_cap);
	player->metadata_observers[0] = status_changed;
	player->metadata_observers_num++;

	res = pthread_cond_init( &(player->load_cond), NULL );
	assert( res == 0 );

	res = pthread_cond_init( &(player->done_track_cond), NULL );
	assert( res == 0 );

	pthread_mutex_init( &player->the_lock, NULL );

	return player;
}

Playlist* setupTestPlaylist()
{
	Playlist *playlist = (Playlist*) malloc(sizeof(Playlist));
	playlist->id = 1,
	playlist->ref_count = 1,
	playlist->name = sdsnew("playlist"),
	playlist->next = playlist->prev = playlist;

	PlaylistItem *last_item = NULL;
	for( int i = 10; i > 0; i-- ) {
		Track *track = (Track*) malloc(sizeof(Track));
		track->artist = sdsnew("artist");
		track->title = sdscatprintf(sdsempty(), "title %d", i);
		track->path = sdsnew("path");
		track->album = sdsnew("album");
		track->track = i;
		track->length = 1.23;
		track->next_ptr = NULL;
		track->color_field = '\0';
		track->left = NULL;
		track->right = NULL;

		PlaylistItem *item = malloc(sizeof(PlaylistItem));
		item->id = 1;
		item->ref_count = 1;
		item->track = track;
		item->is_stream = false;
		item->next = last_item;
		item->parent = playlist;
		last_item = item;
	}
	playlist->root = last_item;
	return playlist;
}

unsigned int testPlayerLoop()
{
	Player *player = setupTestPlayer();
	Playlist *playlist = setupTestPlaylist();

	expected_buffer_size = 1024*1024*1024;
	expected_buffer_read = 0;
	expected_buffer_len = 0;
	expected_buffer = malloc(expected_buffer_size);

	assert( !init_circular_buffer( &player->circular_buffer, 5001 ) );
	assert( !play_queue_init( &player->play_queue ) );

	pthread_t audio_thread;
	assert( !pthread_create( &audio_thread, NULL, (void *) &player_audio_thread_run, (void *) player) );

	int seed = 1;

	PlayQueueItem *pqi;
	char *p;
	size_t buffer_free;
	int chunk_size = 500;
	for( PlaylistItem *current_item = playlist->root; current_item; current_item = current_item->next ) {
		usleep(10000);
		pthread_mutex_lock( &player->the_lock );
		LOG_DEBUG("adding pqi");

		assert( !get_buffer_write( &player->circular_buffer, 1024, &p, &buffer_free ) );

		assert( play_queue_add( &player->play_queue, &pqi ) == 0);
		usleep(10000);
		pqi->buf_start = p;
		pqi->playlist_item = current_item;
		LOG_DEBUG("p=p done adding pqi", p);
		pthread_mutex_unlock( &player->the_lock );

		for( int chunk_num = 0; chunk_num < 10; chunk_num++ ) {
			LOG_DEBUG("chunk=d writing stubbed audio",  chunk_num);
			if( chunk_num > 0 ) {
				LOG_DEBUG("waiting");
				assert( !get_buffer_write( &player->circular_buffer, 1024, &p, &buffer_free ) );
				LOG_DEBUG("waiting done");
			}
			usleep(10000);
			p[0] = AUDIO_DATA;

			size_t payload_size = chunk_size-1-sizeof(size_t);
			memcpy( p+1, &payload_size, sizeof(size_t) );

			set_buffer( p+1+sizeof(size_t), seed, payload_size );
			set_buffer( expected_buffer + expected_buffer_len, seed, payload_size );
			expected_buffer_len += payload_size;
			seed++;

			buffer_mark_written( &player->circular_buffer, chunk_size );
		}
	}

	printf("waiting for audio_consumer calls to complete\n");
	long timeout = time(NULL) + 3;
	while( expected_buffer_read < expected_buffer_len ) {
		usleep(100);
		if( time(NULL) > timeout ) {
			printf("timeout\n");
			assert( 0 );
		}
	}
	assert( expected_buffer_read == expected_buffer_len );

	printf("waiting for join\n");
	player->exit = true;
	wake_up_get_buffer_read( &player->circular_buffer );
	assert( !pthread_join( audio_thread, NULL ) );


	//assert( !start_player( player ) );
	//assert( !stop_player( player ) );

	return 0;
}

typedef struct test_feeder_data {
	PlaylistItem *root;
	Player *player;
} test_feeder_data;

void test_feeder_item(Player *player, PlaylistItem *playlist_item)
{
	int res;
	char *p;
	size_t buffer_free;
	PlayQueueItem *pqi;
	int chunk_size = 500;
	int track_num = playlist_item->track->track;

	pthread_mutex_lock( &player->the_lock );
	LOG_DEBUG("adding pqi");

	for(;;) {
		p = NULL;
		res = get_buffer_write( &player->circular_buffer, 1024, &p, &buffer_free );
		if( res == 0 ) break;
		if( player->load_abort_requested || player->exit ) return;
		usleep(10);
	}

	assert( play_queue_add( &player->play_queue, &pqi ) == 0);
	usleep(1);
	pqi->buf_start = p;
	pqi->playlist_item = playlist_item;
	LOG_DEBUG("p=d done adding pqi", p - player->circular_buffer.p);
	pthread_mutex_unlock( &player->the_lock );

	for( int chunk_num = 0; chunk_num < 10; chunk_num++ ) {
		LOG_DEBUG("chunk=d writing stubbed audio",  chunk_num);
		if( chunk_num > 0 ) {
			LOG_DEBUG("waiting");

			for(;;) {
				p = NULL;
				res = get_buffer_write( &player->circular_buffer, 1024, &p, &buffer_free );
				if( res == 0 ) break;
				if( player->load_abort_requested || player->exit ) return;
				usleep(10);
			}

			LOG_DEBUG("waiting done");
		}
		usleep(1);
		p[0] = AUDIO_DATA;

		size_t payload_size = chunk_size-1-sizeof(size_t);
		memcpy( p+1, &payload_size, sizeof(size_t) );

		set_buffer_track( p+1+sizeof(size_t), track_num, payload_size );
		set_buffer_track( expected_buffer + expected_buffer_len, track_num, payload_size );
		expected_buffer_len += payload_size;

		buffer_mark_written( &player->circular_buffer, chunk_size );
	}
	return;
}

//static void* test_feeder(void *data_void)
//{
//	int res;
//	PlaylistItem *item = NULL;
//	test_feeder_data *data = (test_feeder_data*) data_void;
//
//	Player *player = data->player;
//
//	for(;;) {
//		if( player->exit ) {
//			return NULL;
//		}
//		//LOG_DEBUG("locking - player_reader_thread_run");
//		pthread_mutex_lock( &player->the_lock );
//
//		if( player->load_abort_requested ) {
//			// loader has been told to stop, busy-wait here
//			pthread_mutex_unlock( &player->the_lock );
//			usleep(100);
//			continue;
//		}
//		LOG_DEBUG("I AM HERE");
//
//		if( player->playlist_item_to_buffer_override != NULL ) {
//			player->playlist_item_to_buffer = player->playlist_item_to_buffer_override;
//			player->playlist_item_to_buffer_override = NULL;
//		}
//
//		if( player->playlist_item_to_buffer != NULL ) {
//			item = player->playlist_item_to_buffer;
//			player->load_in_progress = true;
//		} else {
//			item = NULL;
//		}
//
//		//LOG_DEBUG("unlocking - player_reader_thread_run");
//		pthread_mutex_unlock( &player->the_lock );
//
//		if( item == NULL ) {
//			usleep(10000);
//			continue;
//		}
//
//		res = test_feeder_item( data, item, item->track->track );
//		LOG_DEBUG("res=d finished loading track", res);
//
//		pthread_mutex_lock( &player->the_lock );
//		player->load_in_progress = false;
//		pthread_mutex_unlock( &player->the_lock );
//		pthread_cond_signal( &player->load_cond );
//
//		if( player->playlist_item_to_buffer->next ) {
//			player->playlist_item_to_buffer = player->playlist_item_to_buffer->next;
//		} else if( player->playlist_item_to_buffer->parent == NULL ) {
//			assert(0);
//		} else {
//			player->playlist_item_to_buffer = player->playlist_item_to_buffer->parent->root;
//		}
//
//	}
//
//	LOG_DEBUG("done writing stubbed audio");
//	return NULL;
//}

unsigned int testPlayerSkip()
{
	Player *player = setupTestPlayer();
	Playlist *playlist = setupTestPlaylist();
	player->load_item = &test_feeder_item;

	num_skips = 0;
	last_track_seen = 1;
	player->audio_consumer = test_player_consumer_skip;

	expected_buffer_size = 1024*1024*1024;
	expected_buffer_read = 0;
	expected_buffer_len = 0;
	expected_buffer = malloc(expected_buffer_size);

	assert( !init_circular_buffer( &player->circular_buffer, 5001 ) );
	assert( !play_queue_init( &player->play_queue ) );

	pthread_t audio_thread;
	assert( !pthread_create( &audio_thread, NULL, &player_audio_thread_run, (void *) player) );

	pthread_t feeder_thread;
	//test_feeder_data data = {
	//	.root = playlist->root,
	//	.player = player,
	//};
	player->playlist_item_to_buffer = playlist->root;
	assert( !pthread_create( &feeder_thread, NULL, &player_reader_thread_run, (void*) player ) );

	unsigned char wait_until_track = 2;
	printf("here\n");
	while( wait_until_track < 10 ) {
		printf("here1\n");
		bool printed = false;
		while(last_track_seen < wait_until_track) {
			if( !printed ) {
				printf("last_track_seen=%x waiting_until=%x\n", last_track_seen, wait_until_track);
				printed = true;
			}
			usleep(10000);
		}
		wait_until_track = last_track_seen;

		printf("skipping track\n");
		player_change_next_track( player, TRACK_CHANGE_IMMEDIATE );
		printf("skipping track done\n");
		wait_until_track++;
		printf("=last_track_seen=%x waiting_until=%x\n", last_track_seen, wait_until_track);
		
		//player->next_track = true;
		//res = pthread_cond_wait( &player->done_track_cond, &player->the_lock );
		//printf("done track cond finished\n");
		//assert( res == 0 );
		//pthread_mutex_unlock( &player->the_lock );
		//printf("freeing lock\n");
	}


	printf("waiting for audio_consumer calls to complete\n");
	long timeout = time(NULL) + 15;
	while( num_skips < 13 ) {
		usleep(100);
		if( time(NULL) > timeout ) {
			printf("timeout; num_skips: %d\n", num_skips);
			assert( 0 );
		}
	}

	player->exit = true;

	LOG_DEBUG("waiting for audio_thread");
	wake_up_get_buffer_read( &player->circular_buffer );
	assert( !pthread_join( audio_thread, NULL ) );

	LOG_DEBUG("waiting for feeder_thread");
	wake_up_get_buffer_write( &player->circular_buffer );
	assert( !pthread_join( feeder_thread, NULL ) );

	LOG_DEBUG("test complete");
	return 0;
}

