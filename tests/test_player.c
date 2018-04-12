#include "tests.h"

#include "testRunner.h"

#include "player.h"
#include "log.h"

void player_reader_thread_run( void *data );
void player_audio_thread_run( void *data );

char *expected_buffer;
size_t expected_buffer_size;
size_t expected_buffer_read;
size_t expected_buffer_len;

int test_player_consumer(const char *p, size_t n)
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

int test_player_consumer_skip(const char *p, size_t n)
{
	for( int i = 0; i < n; i++ ) {
		unsigned char current_track = p[i];
		if( current_track == last_track_seen || current_track == (last_track_seen+1) ) {
			if( last_track_seen != current_track ) {
				printf( "got data for new track: %x\n", current_track );
				last_track_seen = current_track;
				usleep(1000000);
			}
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
		track->track = 3;
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

	pthread_mutex_init( &player->the_lock, NULL );

	TEST_ASSERT( !init_circular_buffer( &player->circular_buffer, 5001 ) );
	TEST_ASSERT( !play_queue_init( &player->play_queue ) );

	pthread_t audio_thread;
	TEST_ASSERT( !pthread_create( &audio_thread, NULL, (void *) &player_audio_thread_run, (void *) player) );

	int seed = 1;

	PlayQueueItem *pqi;
	char *p;
	size_t buffer_free;
	int chunk_size = 500;
	for( PlaylistItem *current_item = playlist->root; current_item; current_item = current_item->next ) {
		LOG_DEBUG("adding pqi");
		usleep(10000);
		pthread_mutex_lock( &player->the_lock );

		TEST_ASSERT( !get_buffer_write( &player->circular_buffer, 1024, &p, &buffer_free ) );

		TEST_ASSERT( play_queue_add( &player->play_queue, &pqi ) == 0);
		usleep(10000);
		pqi->buf_start = p;
		pqi->playlist_item = current_item;
		pthread_mutex_unlock( &player->the_lock );

		for( int chunk_num = 0; chunk_num < 10; chunk_num++ ) {
			LOG_DEBUG("chunk=d writing stubbed audio",  chunk_num);
			if( chunk_num > 0 ) {
				LOG_DEBUG("waiting");
				TEST_ASSERT( !get_buffer_write( &player->circular_buffer, 1024, &p, &buffer_free ) );
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
			TEST_ASSERT( 0 );
		}
	}
	TEST_ASSERT_EQUALS( expected_buffer_read, expected_buffer_len );

	printf("waiting for join\n");
	player->exit = true;
	wake_up_get_buffer_read( &player->circular_buffer );
	TEST_ASSERT( !pthread_join( audio_thread, NULL ) );


	//TEST_ASSERT( !start_player( player ) );
	//TEST_ASSERT( !stop_player( player ) );

	return 0;
}

static void* test_feeder(void *data)
{
	PlaylistItem *root = (PlaylistItem *data);

	PlayQueueItem *pqi;
	char *p;
	size_t buffer_free;
	int chunk_size = 500;
	unsigned char track_num = 1;
	for( PlaylistItem *current_item = root; current_item; current_item = current_item->next, track_num++ ) {
		LOG_DEBUG("adding pqi");
		usleep(1);
		pthread_mutex_lock( &player->the_lock );

		TEST_ASSERT( !get_buffer_write( &player->circular_buffer, 1024, &p, &buffer_free ) );

		TEST_ASSERT( play_queue_add( &player->play_queue, &pqi ) == 0);
		usleep(1);
		pqi->buf_start = p;
		pqi->playlist_item = current_item;
		pthread_mutex_unlock( &player->the_lock );

		for( int chunk_num = 0; chunk_num < 10; chunk_num++ ) {
			LOG_DEBUG("chunk=d writing stubbed audio",  chunk_num);
			if( chunk_num > 0 ) {
				LOG_DEBUG("waiting");
				TEST_ASSERT( !get_buffer_write( &player->circular_buffer, 1024, &p, &buffer_free ) );
				LOG_DEBUG("waiting done");
			}
			usleep(1);
			p[0] = AUDIO_DATA;

			size_t payload_size = chunk_size-1-sizeof(size_t);
			memcpy( p+1, &payload_size, sizeof(size_t) );

			set_buffer_track( p+1+sizeof(size_t), track_num, payload_size );
			set_buffer_track( expected_buffer + expected_buffer_len, track_num, payload_size );
			expected_buffer_len += payload_size;
			seed++;

			buffer_mark_written( &player->circular_buffer, chunk_size );
		}
	}

	return NULL;
}

unsigned int testPlayerSkip()
{
	Player *player = setupTestPlayer();
	Playlist *playlist = setupTestPlaylist();

	last_track_seen = 1;
	player->audio_consumer = test_player_consumer_skip;

	expected_buffer_size = 1024*1024*1024;
	expected_buffer_read = 0;
	expected_buffer_len = 0;
	expected_buffer = malloc(expected_buffer_size);

	pthread_mutex_init( &player->the_lock, NULL );

	TEST_ASSERT( !init_circular_buffer( &player->circular_buffer, 5001 ) );
	TEST_ASSERT( !play_queue_init( &player->play_queue ) );

	pthread_t audio_thread;
	TEST_ASSERT( !pthread_create( &audio_thread, NULL, (void *) &player_audio_thread_run, (void *) player) );

	pthread_t feeder_thread;
	TEST_ASSERT( !pthread_create( &feeder_thread, NULL, &test_feeder, (void*) playlist->root ) );

	int seed = 1;


	unsigned char wait_until_track = 2;
	printf("here\n");
	while( wait_until_track < 10 ) {
		printf("here1\n");
		while(last_track_seen < wait_until_track) {
			printf("last_track_seen=%x waiting_until=%x\n", last_track_seen, wait_until_track);
			usleep(100);
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
	long timeout = time(NULL) + 3;
	while( expected_buffer_read < expected_buffer_len ) {
		usleep(100);
		if( time(NULL) > timeout ) {
			printf("timeout\n");
			TEST_ASSERT( 0 );
		}
	}
	TEST_ASSERT_EQUALS( expected_buffer_read, expected_buffer_len );

	printf("waiting for join\n");
	player->exit = true;
	wake_up_get_buffer_read( &player->circular_buffer );
	TEST_ASSERT( !pthread_join( audio_thread, NULL ) );
	TEST_ASSERT( !pthread_join( feeder_thread, NULL ) );


	//TEST_ASSERT( !start_player( player ) );
	//TEST_ASSERT( !stop_player( player ) );

	return 0;
}

