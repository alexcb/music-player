#ifdef USE_RASP_PI

#include "raspbpi.h"

#include "timing.h"
#include "log.h"

#include <pthread.h>
#include <wiringPi.h>

#define PIN_RIGHT_UP   9
#define PIN_RIGHT_DOWN 8

#define PIN_LEFT_UP   0
#define PIN_LEFT_DOWN 2

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
	cur_switch_left_up = digitalRead( PIN_LEFT_UP );
	cur_switch_left_down = digitalRead( PIN_LEFT_DOWN );
	cur_switch_right_up = digitalRead( PIN_RIGHT_UP );
	cur_switch_right_down = digitalRead( PIN_RIGHT_DOWN );
	pthread_cond_signal( &gpio_input_changed_cond );
}

void* gpio_input_thread_run( void *p )
{
	//long current_time;
	int res;
	Player *player = (Player*) p;
	pthread_mutex_t mutex;
	pthread_mutex_init( &mutex, NULL );
	pthread_mutex_lock( &mutex );
	LOG_DEBUG("gpio_input_thread_run started");
	for(;;) {
		res = pthread_cond_wait( &gpio_input_changed_cond, &mutex );
		if( res ) {
			LOG_ERROR("pthread returned error");
			return NULL;
		}
		if( cur_switch_left_up != last_switch_left_up ) {
			LOG_DEBUG("TOGGLE left up");
			last_switch_left_up = cur_switch_left_up;
			if( cur_switch_left_up ) {
				player_pause( player );
			}
		}
		if( cur_switch_left_down != last_switch_left_down ) {
			LOG_DEBUG("TOGGLE left down");
			last_switch_left_down = cur_switch_left_down;
			if( cur_switch_left_down ) {
				player_pause( player );
			}
		}
		if( cur_switch_right_up != last_switch_right_up ) {
			LOG_DEBUG("TOGGLE right up");
			last_switch_right_up = cur_switch_right_up;

			if( cur_switch_right_up ) {
				res = player_change_next_track( player, TRACK_CHANGE_IMMEDIATE );
				if( res != 0 ) {
					LOG_ERROR("err=d failed to change to next album", res);
				}
			}

		}

		if( cur_switch_right_down != last_switch_right_down ) {
			LOG_DEBUG("TOGGLE right down");
			last_switch_right_down = cur_switch_right_down;

			if( cur_switch_right_down ) {
				res = player_change_next_album( player, TRACK_CHANGE_IMMEDIATE );
				if( res != 0 ) {
					LOG_ERROR("err=d failed to change to next album", res);
				}
			}
		}
			//last_play_switch = cur_play_switch;

			//current_time = get_current_time_ms();
			//if( player_get_control(player) & PLAYER_CONTROL_PLAYING ) {
			//	// was just switched on
			//	long diff = current_time - last_toggle_time;
			//	if( diff < 2000 ) {
			//		LOG_INFO("diff=d fast play toggles detected, skipping to next artist", diff);
			//		res = player_change_next_album( player, TRACK_CHANGE_IMMEDIATE );
			//		if( res != 0 ) {
			//			LOG_ERROR("err=d failed to change to next album", res);
			//		}
			//	}
			//}
			//last_toggle_time = current_time;
		//}

		//data->player->playing = play_switch;
		//if( rotation_switch > 0 ) {
		//	playlist_manager_next( data->playlist_manager );
		//} else if( rotation_switch < 0 ) {
		//	playlist_manager_prev( data->playlist_manager );
		//}
		//rotation_switch = 0;
	}
	return NULL;
}

int init_rasp_pi(Player *player) {
	int res;
	LOG_INFO("initializing raspberry pi settings");

	pthread_cond_init( &gpio_input_changed_cond, NULL );

	if( wiringPiSetup() == -1 ) {
		LOG_ERROR("failed to setup wiringPi");
		return 1;
	}

	res = pthread_create( &gpio_input_thread, NULL, &gpio_input_thread_run, (void*) player );
	if( res ) {
		LOG_ERROR("failed to create input thread");
		return 1;
	}

	// on/off switch
	pinMode( PIN_RIGHT_DOWN, INPUT );
	pinMode( PIN_RIGHT_UP,   INPUT );
	pinMode( PIN_LEFT_DOWN,  INPUT );
	pinMode( PIN_LEFT_UP,    INPUT );
	pullUpDnControl( PIN_RIGHT_DOWN, PUD_UP );
	pullUpDnControl( PIN_RIGHT_UP,   PUD_UP );
	pullUpDnControl( PIN_LEFT_DOWN,  PUD_UP );
	pullUpDnControl( PIN_LEFT_UP,    PUD_UP );
	
	last_switch_left_up    = cur_switch_left_up    = digitalRead( PIN_LEFT_UP    );
	last_switch_left_down  = cur_switch_left_down  = digitalRead( PIN_LEFT_DOWN  );
	last_switch_right_up   = cur_switch_right_up   = digitalRead( PIN_RIGHT_UP   );
	last_switch_right_down = cur_switch_right_down = digitalRead( PIN_RIGHT_DOWN );
	//
	//last_play_switch = cur_play_switch = digitalRead( PIN_SWITCH );

	wiringPiISR( PIN_RIGHT_UP,   INT_EDGE_BOTH, switchIntHandler);
	wiringPiISR( PIN_RIGHT_DOWN, INT_EDGE_BOTH, switchIntHandler);
	wiringPiISR( PIN_LEFT_UP,    INT_EDGE_BOTH, switchIntHandler);
	wiringPiISR( PIN_LEFT_DOWN,  INT_EDGE_BOTH, switchIntHandler);

	return 0;
}

#endif
