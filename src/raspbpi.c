#ifdef USE_RASP_PI

#	include "raspbpi.h"

#	include "log.h"
#	include "timing.h"

#	include <pthread.h>
#	include <wiringPi.h>

// see http://wiringpi.com/pins/ for mappings
#	define PLAYING_OUTPUT_PIN 3

// switch must remain in this state for at least this many ms
#	define MIN_CONSTANT_STATE_MS 10

#define HOLD_TIME_MS 800

struct gpio_switch
{
	int gpio_pin;
	int current_state;
	int held_called;

	uint64_t changed_at;

	// for de-bouncing
	int last_state;
	uint64_t last_state_t;

	// after being de-bounced
	int last_actual_state;
	uint64_t last_actual_state_t;

	// immediately called on down
	void ( *on_down )( Player* p );

	// immediately called on up
	void ( *on_up )( Player* p );

	// called if held down for 2 seconds
	void ( *on_hold )( Player* p );
};

void next_track( Player* player )
{
	int res;
	res = player_change_next_track( player, TRACK_CHANGE_IMMEDIATE );
	if( res != 0 ) {
		LOG_ERROR( "err=d failed to change to next track", res );
	}
}

void next_album( Player* player )
{
	int res;
	res = player_change_next_album( player, TRACK_CHANGE_IMMEDIATE );
	if( res != 0 ) {
		LOG_ERROR( "err=d failed to change to next album", res );
	}
}

void next_playlist( Player* player )
{
	player_change_next_playlist( player, TRACK_CHANGE_IMMEDIATE );
}

void do_play( Player* player )
{
	player_set_playing( player, 1 );
}

void do_pause( Player* player )
{
	player_set_playing( player, 0 );
}

struct gpio_switch switches[] = {
// see http://wiringpi.com/pins/ for pinouts
#	ifdef KITCHEN
	// switch
	{2, 0, 0, 0, 0, 0, 0, 0, &do_pause, &do_play, NULL},

	// left button
	{0, 0, 0, 0, 0, 0, 0, 0, NULL, &next_playlist, &player_say_what},

	// middle button
	{8, 0, 0, 0, 0, 0, 0, 0, NULL, &next_album, &player_say_what},

	// right button
	{9, 0, 0, 0, 0, 0, 0, 0, NULL, &next_track, &player_say_what}
#	else
	// right up
	{9, 0, 0, 0, 0, 0, 0, 0, NULL, &next_album, NULL},

	//right down
	{8, 0, 0, 0, 0, 0, 0, 0, NULL, &next_track, NULL},

	// left up
	{0, 0, 0, 0, 0, 0, 0, 0, NULL, &next_playlist, NULL},

	// left down
	{2, 0, 0, 0, 0, 0, 0, 0, NULL, &player_pause, &player_say_what}
#	endif // KITCHEN
};
int num_switches = sizeof( switches ) / sizeof( struct gpio_switch );

pthread_t gpio_input_thread;
pthread_cond_t gpio_input_changed_cond;

int last_switch_right_up;
int last_switch_right_down;
int last_switch_left_up;
int last_switch_left_down;

int cur_switch_right_up;
int cur_switch_right_down;
int cur_switch_left_up;
int cur_switch_left_down;

long last_toggle_time = 0;

void switchIntHandler()
{
	for( int i = 0; i < num_switches; i++ ) {
		switches[i].current_state = digitalRead( switches[i].gpio_pin );
	}
	pthread_cond_signal( &gpio_input_changed_cond );
}

void* gpio_input_thread_run( void* p )
{
	//long current_time;
	int res;
	Player* player = (Player*)p;
	pthread_mutex_t mutex;
	pthread_mutex_init( &mutex, NULL );
	pthread_mutex_lock( &mutex );
	LOG_DEBUG( "gpio_input_thread_run started" );

	struct timespec now;
	struct timespec wait_time;
	wait_time.tv_sec = time( NULL );
	wait_time.tv_nsec = 0;

	uint64_t now_t;

	for( ;; ) {
		res = pthread_cond_timedwait( &gpio_input_changed_cond, &mutex, &wait_time );
		if( res && res != ETIMEDOUT ) {
			LOG_ERROR( "res=d pthread_cond_timedwait returned error", res );
			return NULL;
		}

		if( ( res = clock_gettime( CLOCK_PROCESS_CPUTIME_ID, &now ) ) ) {
			LOG_ERROR( "res=d clock_gettime failed" );
			assert( 0 );
		}
		now_t = timespec_to_ms( &now );
		assert( now_t > 0 );

		wait_time = now;
		add_ms( &wait_time, 1 );

		for( int i = 0; i < num_switches; i++ ) {
			int current_state = switches[i].current_state;

			if( switches[i].last_state_t == 0 ) {
				continue; // not yet initted
			}

			if( switches[i].last_state != switches[i].current_state ) {
				// switch has bounced, reset timmer
				switches[i].last_state = current_state;
				switches[i].last_state_t = now_t;
				LOG_INFO( "switch=d new_state=d bounce-reset", i, current_state );
				continue;
			}

			uint64_t ms_since_last_read = now_t - switches[i].last_state_t;
			if( ms_since_last_read < MIN_CONSTANT_STATE_MS ) {
				//LOG_INFO( "switch=d since=l waiting", i, ms_since_last_read );
				continue; //wait until MIN_CONSTANT_STATE_MS
			}

			if( switches[i].last_actual_state != current_state ) {

				LOG_INFO( "switch=d state=d wait=l now=l msg",
						  i,
						  current_state,
						  ms_since_last_read,
						  now_t );
				switches[i].last_actual_state = current_state;
				switches[i].changed_at = now_t;
				if( !current_state ) {
					// 0 = switch has been closed (i.e. pressed)
					LOG_INFO( "switch=d on_down", i );
					if( switches[i].on_down ) {
						switches[i].on_down( player );
					}
				}
				else {
					// 1 = switch is open (i.e. not pressed)
					if( !switches[i].held_called ) {
						LOG_INFO( "switch=d on_up", i );
						if( switches[i].on_up ) {
							switches[i].on_up( player );
						}
					}
					switches[i].held_called = 0;
				}
			} else {
				//if( i == 3 ) {
				//	LOG_INFO("switch=d elapsed=l here", current_state, switches[i].changed_at);
				//}
				if( !current_state && switches[i].changed_at && !switches[i].held_called ) {
					uint64_t elapsed_time = now_t - switches[i].changed_at;
					//LOG_INFO("switch=d elapsed=l on hold?", i, elapsed_time);
					if( elapsed_time >= HOLD_TIME_MS  ) {
						if( switches[i].on_hold ) {
							LOG_INFO( "switch=d on_hold", i );
							switches[i].on_hold( player );
							switches[i].held_called = 1; // stop further triggers
						}
					}
				}
			}
		}
	}
	return NULL;
}

int init_rasp_pi( Player* player )
{
	int res;
	LOG_INFO( "initializing raspberry pi settings" );

	pthread_cond_init( &gpio_input_changed_cond, NULL );

	if( wiringPiSetup() == -1 ) {
		LOG_ERROR( "failed to setup wiringPi" );
		return 1;
	}

	res = pthread_create( &gpio_input_thread, NULL, &gpio_input_thread_run, (void*)player );
	if( res ) {
		LOG_ERROR( "failed to create input thread" );
		return 1;
	}

	uint64_t now_t = get_current_time_ms();
	if( now_t == 0 ) {
		now_t = 1;// must be set
	}

	for( int i = 0; i < num_switches; i++ ) {
		pinMode( switches[i].gpio_pin, INPUT );
		pullUpDnControl( switches[i].gpio_pin, PUD_UP );
		switches[i].last_actual_state = switches[i].last_state = switches[i].current_state =
			digitalRead( switches[i].gpio_pin );
		switches[i].last_state_t = now_t;
		wiringPiISR( switches[i].gpio_pin, INT_EDGE_BOTH, switchIntHandler );
		LOG_INFO( "switch=d state=d init state", i, switches[i].last_state );
	}

#	ifdef KITCHEN
#	else
	// stereo relay
	LOG_INFO( "set low" );
	pinMode( PLAYING_OUTPUT_PIN, OUTPUT );
	digitalWrite( PLAYING_OUTPUT_PIN, HIGH );
#	endif

	return 0;
}

void rpi_set_status( bool playing )
{
	if( playing ) {
		LOG_INFO( "set high" );
		digitalWrite( PLAYING_OUTPUT_PIN, LOW ); // low = ground, which toggles the relay
	}
	else {
		LOG_INFO( "set low" );
		digitalWrite( PLAYING_OUTPUT_PIN, HIGH );
	}
}

#endif
