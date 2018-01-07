#ifdef USE_RASP_PI

#include "raspbpi.h"

#include "log.h"

#include <pthread.h>
#include <wiringPi.h>

#define PIN_ROTARY_1 8
#define PIN_ROTARY_2 9
#define PIN_SWITCH 7

#define PIN_LCD_DC 4
#define PIN_LCD_RST 5
#define PIN_LED 1

#define LED_VALUE 256
#define CONTRAST 0xAA


#define FONT_BLOCK_SIZE 8
#define LCD_WIDTH 84
#define LCD_HEIGHT 48

#define BITS 8

pthread_t gpio_input_thread;
pthread_cond_t gpio_input_changed_cond;

int last_play_switch;
int cur_play_switch;

void switchIntHandler()
{
	cur_play_switch = digitalRead( PIN_SWITCH );
	pthread_cond_signal( &gpio_input_changed_cond );
}

void* gpio_input_thread_run( void *p )
{
	int res;
	//MyData *data = (MyData*) p;
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
		LOG_DEBUG("cur=d last=d process GPIO input", cur_play_switch, last_play_switch);
		if( last_play_switch != cur_play_switch ) {
			LOG_DEBUG("TOGGLE PAUSE");
			last_play_switch = cur_play_switch;
		}

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

int init_rasp_pi() {
	int res;
	LOG_INFO("initializing raspberry pi settings");

	if( wiringPiSetup() == -1 ) {
		LOG_ERROR("failed to setup wiringPi");
		return 1;
	}

	res = pthread_create( &gpio_input_thread, NULL, &gpio_input_thread_run, NULL );
	if( res ) {
		LOG_ERROR("failed to create input thread");
		return 1;
	}

	// on/off switch
	pinMode( PIN_SWITCH, INPUT );
	pullUpDnControl( PIN_SWITCH, PUD_UP );
	last_play_switch = cur_play_switch = digitalRead( PIN_SWITCH );

	return 0;
}

#endif
