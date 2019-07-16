#ifdef USE_RASP_PI

#include "raspbpi.h"

#include "timing.h"
#include "log.h"

#include <pthread.h>
#include <wiringPi.h>

struct gpio_switch {
	int gpio_pin;
	int last_state;
	int current_state;
	int changed_at;
	int held;

	// immediately called on down
	void (*on_down)(Player* p);

	// immediately called on up
	void (*on_up)(Player* p);

	// called if held down for 2 seconds
	void (*on_hold)(Player* p);
};

void next_track(Player *player)
{
	int res;
	res = player_change_next_track( player, TRACK_CHANGE_IMMEDIATE );
	if( res != 0 ) {
		LOG_ERROR("err=d failed to change to next track", res);
	}
}

void next_album(Player *player)
{
	int res;
	res = player_change_next_album( player, TRACK_CHANGE_IMMEDIATE );
	if( res != 0 ) {
		LOG_ERROR("err=d failed to change to next album", res);
	}
}

void next_playlist(Player *player)
{
	player_change_next_playlist( player, TRACK_CHANGE_IMMEDIATE );
}

void do_play(Player *player)
{
	player_set_playing( player, 1 );
}

void do_pause(Player *player)
{
	player_set_playing( player, 0 );
}

struct gpio_switch switches[] = {
#ifdef KITCHEN
	// switch
	{2, 0, 0, 0, 0, &do_pause, &do_play, 0},

	// left button
	{0, 0, 0, 0, 0, 0, &next_playlist, &player_say_what},

	// middle button
	{8, 0, 0, 0, 0, 0, &next_album, &player_say_what},
	
	// right button
	{9, 0, 0, 0, 0, 0, &next_track, &player_say_what}
#else
	// right up
	{9, 0, 0, 0, 0, 0, &next_album, 0},

	//right down
	{8, 0, 0, 0, 0, 0, &next_track, 0},

	// left up
	{0, 0, 0, 0, 0, 0, &next_playlist, 0},

	// left down
	{2, 0, 0, 0, 0, 0, &player_pause, &player_say_what}
#endif // KITCHEN
};
int num_switches = sizeof(switches)/sizeof(struct gpio_switch);

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
	for(int i = 0; i < num_switches; i++ ) {
		switches[i].current_state = digitalRead( switches[i].gpio_pin );
	}
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

    struct timespec now;
    struct timespec wait_time;
    wait_time.tv_sec = time(NULL);
    wait_time.tv_nsec = 0; //100*1000//100ms

	for(;;) {
		res = pthread_cond_timedwait( &gpio_input_changed_cond, &mutex, &wait_time );
		if( res && res != ETIMEDOUT) {
			LOG_ERROR("res=d pthread returned error", res);
			return NULL;
		}

		assert( clock_gettime( CLOCK_PROCESS_CPUTIME_ID, &now ) == 0);

		// TODO there's gotta be a function to do this.
		// something like? https://lists.freedesktop.org/archives/wayland-devel/2017-December/036174.html
		if( now.tv_nsec > 950000 ) {
			wait_time.tv_sec = now.tv_sec + 1;
			wait_time.tv_nsec = 0;
		} else {
			wait_time.tv_sec = now.tv_sec;
			wait_time.tv_nsec = wait_time.tv_nsec + 49999;
		}

		for(int i = 0; i < num_switches; i++ ) {
			int current_state = switches[i].current_state;
			if( switches[i].last_state != current_state ) {
				LOG_INFO("switch=d state=d msg", i, current_state);
				switches[i].last_state = current_state;
				switches[i].changed_at = now.tv_sec;
				if( !current_state ) {
					// 0 = switch has been closed (i.e. pressed)
					LOG_INFO("switch=d on_down", i);
					if( switches[i].on_down ) {
						switches[i].on_down( player );
					}
				} else {
					// 1 = switch is open (i.e. not pressed)
					LOG_INFO("switch=d on_up", i);
					if( switches[i].on_up && switches[i].held == 0) {
						switches[i].on_up( player );
					}
					switches[i].held = 0;
				}
			}
			if( !current_state && switches[i].changed_at ) {
				time_t elapsed_time = now.tv_sec - switches[i].changed_at;
				//LOG_INFO("switch=d elapsed=d on hold?", i, elapsed_time);
				if( elapsed_time >= 2 && switches[i].held == 0) {
					LOG_INFO("switch=d on_hold", i);
					if( switches[i].on_hold ) {
						switches[i].on_hold( player );
						switches[i].held = 1; // stop further triggers
					}
				}
			}
			
		}
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

	for(int i = 0; i < num_switches; i++ ) {
		pinMode( switches[i].gpio_pin, INPUT );
		pullUpDnControl( switches[i].gpio_pin, PUD_UP );
		switches[i].last_state = switches[i].current_state = digitalRead( switches[i].gpio_pin );
		wiringPiISR( switches[i].gpio_pin,  INT_EDGE_BOTH, switchIntHandler);
		LOG_INFO("switch=d state=d init state", i, switches[i].last_state);
	}

	return 0;
}

#endif
