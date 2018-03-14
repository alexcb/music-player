#include "tests.h"

#include "testRunner.h"

#include "player.h"

void player_reader_thread_run( void *data );
void player_audio_thread_run( void *data );

unsigned int testPlayerLoop()
{
	Player player = {
		.pa_handle = NULL,
		.alsa_handle = NULL,
		.next_track = NULL,

		.next_track = NULL,
		.load_in_progress = false,
		.load_abort_requested = false,

		.playing = true,
		.current_track = NULL,

		.playlist_item_to_buffer = NULL,
		.playlist_item_to_buffer_override = NULL,
	};

	pthread_mutex_init( &player.the_lock, NULL );

	TEST_ASSERT( !init_circular_buffer( &player.circular_buffer, 10*1024*1024 ) );
	TEST_ASSERT( !play_queue_init( &player.play_queue ) );

	TEST_ASSERT( !pthread_create( &player.reader_thread, NULL, (void *) &player_audio_thread_run, (void *) &player) );

	char *p;
	size_t buffer_free;
	TEST_ASSERT( !get_buffer_write( &player.circular_buffer, 1024, &p, &buffer_free ) );
	buffer_mark_written( &player.circular_buffer, 500 );


	TEST_ASSERT( !pthread_join( player.reader_thread, NULL ) );


	//TEST_ASSERT( !start_player( &player ) );
	//TEST_ASSERT( !stop_player( &player ) );

	return 0;
}

