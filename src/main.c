#include <ao/ao.h>
#include <mpg123.h>
#include <string.h>
#include <wiringPi.h>
#include <assert.h>
#include <font8x8_basic.h>
#include <unistd.h>
#include <time.h>

#include <dirent.h> 


#include "httpget.h"
#include "lcd.h"
#include "rotary.h"
#include "player.h"
#include "albums.h"
#include "playlist_manager.h"
#include "playlist.h"
#include "string_utils.h"
#include "log.h"
#include "errors.h"
#include "my_data.h"
#include "web.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


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

void update_metadata_lcd(bool playing, const PlayerTrackInfo *track, void *data);

//typedef struct Playlist
//{
//	const char *name;
//	const char *url;
//} Playlist;
//Playlist playlists[] = {
//	{
//		"kexp",
//		"http://live-mp3-128.kexp.org:80/kexp128.mp3"
//	},
//	{
//		"kcrw",
//		"http://kcrw.streamguys1.com/kcrw_192k_mp3_e24_internet_radio"
//	},
//	{
//		"witr",
//		"http://streaming.witr.rit.edu:8000/witr-undrgnd-mp3-192"
//	},
//	{
//		"mp3 list",
//		"/home/alex/foo.m3u"
//	}
//};

// can be changed in interupt handlers, must be volitile
volatile int rotation_switch = 0;
volatile int play_switch = 0;

pthread_t gpio_input_thread;
pthread_cond_t gpio_input_changed_cond;


RotaryState rotary_state;

void rotaryIntHandler()
{
	int pin1 = digitalRead( PIN_ROTARY_1 );
	int pin2 = digitalRead( PIN_ROTARY_2 );
	int res = updateRotary(&rotary_state, pin1, pin2);
	if( res > 0 ) {
		LOG_DEBUG( "clockwise" );
		rotation_switch = 1;
		pthread_cond_signal( &gpio_input_changed_cond );
	} else if( res < 0 ) {
		LOG_DEBUG( "counter-clockwise" );
		rotation_switch = -1;
		pthread_cond_signal( &gpio_input_changed_cond );
	}
}

void switchIntHandler()
{
	play_switch = digitalRead( PIN_SWITCH );
	pthread_cond_signal( &gpio_input_changed_cond );
}



struct httpdata hd;

char img_data[504] = {0x1, 0x0, 0x0, 0x0, 0x0, 0xa4, 0x2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x4, 0x0, 0x0, 0x0, 0x0, 0x0, 0x4, 0x0, 0x0, 0x0, 0x0, 0x0, 0x4, 0x0, 0x0, 0x0, 0x0, 0x0, 0x4, 0x0, 0x0, 0x0, 0x0, 0x0, 0x4, 0x0, 0xfc, 0xf, 0x0, 0x0, 0x4, 0x80, 0x1f, 0x10, 0x0, 0xc, 0x4, 0xf0, 0xd, 0x10, 0x0, 0x38, 0x8, 0x18, 0x2, 0x10, 0x0, 0x60, 0x8, 0x8, 0x3, 0x10, 0x0, 0xc0, 0x9, 0xb6, 0x1, 0x10, 0x0, 0x0, 0x6, 0xd2, 0x0, 0x10, 0x0, 0x0, 0x0, 0x79, 0x0, 0x10, 0x0, 0x0, 0x83, 0x20, 0x0, 0x10, 0x0, 0xc0, 0x40, 0x30, 0x0, 0x10, 0x0, 0x20, 0x20, 0x1a, 0x0, 0x10, 0x0, 0x18, 0x30, 0x9, 0x0, 0x10, 0x0, 0x0, 0x10, 0xd, 0x0, 0x10, 0x0, 0x0, 0x88, 0x4, 0x0, 0x10, 0x0, 0x0, 0x48, 0x6, 0x0, 0x10, 0x0, 0x0, 0x4c, 0x3, 0x0, 0x10, 0x0, 0x0, 0x44, 0x1, 0x0, 0x10, 0x0, 0x0, 0x82, 0x1, 0x0, 0x10, 0x0, 0x0, 0x82, 0x0, 0x0, 0x10, 0x0, 0x0, 0x93, 0x0, 0x0, 0x10, 0x0, 0x0, 0x91, 0x0, 0x0, 0x10, 0x0, 0x0, 0x91, 0x0, 0x0, 0x10, 0x0, 0x0, 0xd1, 0xfc, 0xb, 0x10, 0x0, 0x80, 0x59, 0x40, 0x88, 0x13, 0x0, 0x80, 0x48, 0x40, 0x78, 0x10, 0x0, 0x80, 0x48, 0xc0, 0x1b, 0x10, 0x0, 0x80, 0x22, 0xfc, 0x8, 0x10, 0x0, 0x80, 0x22, 0x0, 0x8, 0x10, 0x0, 0x80, 0x22, 0x0, 0xa, 0x14, 0x0, 0xc0, 0x22, 0x0, 0x3, 0x13, 0x0, 0x40, 0x2a, 0xe0, 0xc1, 0x11, 0x0, 0x40, 0x2e, 0x78, 0x60, 0x11, 0x0, 0x40, 0x28, 0x46, 0x18, 0x11, 0x0, 0x60, 0x21, 0xfe, 0x38, 0x11, 0x0, 0x20, 0x31, 0x80, 0xc3, 0x13, 0x0, 0x20, 0x15, 0x0, 0x0, 0x16, 0x0, 0x20, 0x15, 0xfc, 0x0, 0x10, 0x0, 0x20, 0x15, 0xfc, 0x3, 0x10, 0x0, 0x30, 0x17, 0x64, 0xf0, 0x17, 0x0, 0x10, 0x14, 0x24, 0x10, 0x14, 0x0, 0x90, 0x10, 0x3c, 0x10, 0x14, 0x0, 0x50, 0x10, 0x0, 0x10, 0x14, 0x0, 0x50, 0x11, 0xfc, 0x17, 0x14, 0x0, 0x50, 0x11, 0x24, 0x30, 0x16, 0x0, 0xd0, 0x12, 0x24, 0x0, 0x10, 0x0, 0x90, 0x12, 0x24, 0xc0, 0x17, 0x0, 0x10, 0x16, 0x3c, 0x70, 0x14, 0x0, 0x30, 0x30, 0x0, 0x10, 0x14, 0x0, 0x20, 0x23, 0x38, 0x10, 0x14, 0x0, 0x40, 0x42, 0x60, 0x30, 0x14, 0x0, 0x80, 0xc0, 0xc0, 0x27, 0x13, 0x0, 0x0, 0x99, 0x30, 0xe0, 0x11, 0x0, 0x0, 0x33, 0x9, 0x0, 0x10, 0x0, 0x0, 0x36, 0x2, 0x0, 0x10, 0x0, 0x0, 0x4, 0xc, 0x0, 0x10, 0x0, 0x0, 0x8c, 0x18, 0x0, 0x10, 0x0, 0x0, 0x90, 0x33, 0x0, 0x10, 0x0, 0x0, 0x20, 0xc1, 0x0, 0x10, 0x0, 0x0, 0x20, 0x80, 0x0, 0x10, 0x0, 0x0, 0x40, 0x98, 0x0, 0x10, 0x0, 0x0, 0x83, 0x31, 0x1, 0x10, 0x0, 0x0, 0x1, 0x2, 0x3, 0x10, 0x0, 0xc0, 0x0, 0x1c, 0x1e, 0x10, 0x0, 0x20, 0x3, 0xf0, 0x70, 0x10, 0x0, 0x10, 0x1, 0x80, 0x81, 0x13, 0x0, 0x18, 0x5, 0x0, 0x7, 0xe, 0x0, 0x88, 0x4, 0x0, 0xfc, 0xf, 0x0, 0x80, 0x2, 0x0, 0x0, 0x0, 0x0, 0x40, 0x2, 0x0, 0x0, 0x0, 0x0, 0x40, 0x2, 0x0, 0x0, 0x0, 0x0, 0x40, 0x2, 0x0, 0x0, 0x0, 0x0, 0x40, 0x2, 0x0, 0x0, 0x0, 0x0, 0x40, 0x6, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

char img_buffer[504];

char text_buf[1024];

void clearLCDBuffer(char *buffer)
{
	memset(img_buffer, 0, 504);
}

void addLCDPixel(char *buffer, int x, int y)
{
	if( 0 <= y && y < LCD_HEIGHT && 0 <= x && x < LCD_WIDTH ) {
		int lcd_height = 48;
		int lcd_pos = x * lcd_height + y;
		int lcd_offset = lcd_pos / 8;
		unsigned char lcd_mask = 1 << (lcd_pos % 8);

		assert( lcd_offset < 504 );
		img_buffer[lcd_offset] = img_buffer[lcd_offset] | lcd_mask;
	}
}

void addLCDChar(char *buffer, int x_offset, int y_offset, char c)
{
	char *bitmap = font8x8_basic[(int)c];
	int set;
	for( int y = 0; y < 8; y++) {
		for( int x = 0; x < 8; x++ ) {
			set = bitmap[y] & 1 << x;
			if( set ) {
				addLCDPixel( buffer, x + x_offset, y + y_offset );
			}
		}
	}
}

void writeText(LCDState *lcd_state, const char *text)
{
	clearLCDBuffer(img_buffer);
	int x = 0;
	int y = 0;
	for( const char *p = text; *p; p++ ) {
		addLCDChar(img_buffer, x, y, *p);
		x += FONT_BLOCK_SIZE;
		if( ( x + FONT_BLOCK_SIZE ) >= LCD_WIDTH ) {
			x = 0;
			y += FONT_BLOCK_SIZE;
		}
		
	}
	//clearLCDBuffer( img_buffer );
	//addLCDPixel( img_buffer, LCD_WIDTH - 1, LCD_HEIGHT - 1 );
	draw_image( lcd_state, (uint8_t *)img_buffer );
}

void setupLCDPins(LCDState *lcd_state)
{
	//# White backlight
	//CONTRAST = 0xaa
	//
	//ROWS = 6
	//COLUMNS = 14
	//PIXELS_PER_ROW = 6
	//ON = 1
	//OFF = 0
	//
	//#gpio's :
	//DC   = 4 # gpio pin 16 = wiringpi no. 4 (BCM 23)
	//RST  = 5 # gpio pin 18 = wiringpi no. 5 (BCM 24)
	//LED  = 1 # gpio pin 12 = wiringpi no. 1 (BCM 18)
	//
	//# SPI connection
	//SCE  = 10 # gpio pin 24 = wiringpi no. 10 (CE0 BCM 8) 
	//SCLK = 14 # gpio pin 23 = wiringpi no. 14 (SCLK BCM 11)
	//DIN  = 12 # gpio pin 19 = wiringpi no. 12 (MOSI BCM 10)

	pinMode( PIN_LCD_DC, OUTPUT );
	pinMode( PIN_LCD_RST, OUTPUT );


    //# if LED == 1 set pin mode to PWM else set it to OUT
	//TODO add led pin to something else   assert( PIN_LED == 1 );
	//TODO add led pin to something else   pinMode( PIN_LED, 2);
	//TODO add led pin to something else cant use pwm and audio at the same time pwmWrite( PIN_LED, 0);

	//# Toggle RST low to reset.
	digitalWrite( PIN_LCD_RST, 0 );
	delay(100);
	digitalWrite( PIN_LCD_RST, 1 );

	init_lcd( lcd_state, SPIDEV0, PIN_LCD_DC, PIN_LCD_RST, PIN_LED );
	set_contrast( lcd_state, CONTRAST );
}

int get_album_id3_data( mpg123_handle *mh, const char *path, char *artist, char *album )
{
	int res;
	LOG_DEBUG( "path=s open", path );
	res = mpg123_open( mh, path );
	if( res != MPG123_OK ) {
		LOG_DEBUG("err=d open error", res);
		res = 1;
		goto error;
	}
	LOG_DEBUG( "seek" );
	mpg123_seek( mh, 0, SEEK_SET );

	LOG_DEBUG( "reading id3" );
	mpg123_id3v1 *v1;
	mpg123_id3v2 *v2;
	res = 1;
	int meta = mpg123_meta_check( mh );
	if( meta & MPG123_NEW_ID3 ) {
		if( mpg123_id3( mh, &v1, &v2 ) == MPG123_OK ) {
			res = 0;
			if( v2 != NULL ) {
				LOG_DEBUG( "populating metadata with id3 v2" );
				if( v2->artist )
					strcpy( artist, v2->artist->p );
				if( v2->album )
					strcpy( album, v2->album->p );
			} else if( v1 != NULL ) {
				LOG_DEBUG( "populating metadata with id3 v1" );
				strcpy( artist, v1->artist );
				strcpy( album, v1->album );
			} else {
				assert( false );
			}
		}
	}

error:
	mpg123_close( mh );
	return res;
}

int get_album_id3_data_from_album_path( mpg123_handle *mh, const char *path, char *artist, char *album )
{
	struct dirent *entry;
	DIR *dir = opendir( path );

	char filepath[1024];

	while( (entry = readdir( dir )) != NULL) {
		if( entry->d_type == DT_REG ) {
			if( has_suffix( entry->d_name, ".mp3" ) ) {
				sprintf( filepath, "%s/%s", path, entry->d_name );
				LOG_DEBUG("path=s attempting to read id3", filepath);
				get_album_id3_data( mh, filepath, artist, album );
				break;
			}
		}
	}
	closedir( dir );
	return 0;
}

int load_albums( AlbumList *album_list, const char *path, mpg123_handle *mh )
{
	int res;
	char artist_path[1024];
	struct dirent *artist_dirent, *album_dirent;
	char artist[1024];
	char album[1024];

	DIR *artist_dir = opendir(path);
	if( artist_dir == NULL ) {
		LOG_ERROR("path=s err=s opendir failed", path, strerror(errno));
		return FILESYSTEM_ERROR;
	}

	int max_load = 3;

	while( (artist_dirent = readdir(artist_dir)) != NULL) {
		if( artist_dirent->d_type != DT_DIR || strcmp(artist_dirent->d_name, ".") == 0 || strcmp(artist_dirent->d_name, "..") == 0 ) {
			continue;
		}
		
		sprintf(artist_path, "%s/%s", path, artist_dirent->d_name);
		LOG_DEBUG("path=s opening dir", artist_path);
		DIR *album_dir = opendir(artist_path);
		if( artist_dir == NULL ) {
			LOG_ERROR("path=s err=s opendir failed", artist_path, strerror(errno));
			return FILESYSTEM_ERROR;
		}

		while( (album_dirent = readdir( album_dir )) != NULL) {
			if( album_dirent->d_type != DT_DIR || strcmp(album_dirent->d_name, ".") == 0 || strcmp(album_dirent->d_name, "..") == 0 ) {
				continue;
			}

			// this is actually now the path to the album
			sprintf(artist_path, "%s/%s/%s", path, artist_dirent->d_name, album_dirent->d_name);

			strcpy( artist, artist_dirent->d_name );
			strcpy( album, album_dirent->d_name );
			get_album_id3_data_from_album_path( mh, artist_path, artist, album );

			res = album_list_add( album_list, artist, album, artist_path );
			if( res ) {
				return res;
			}
			
		}
		closedir( album_dir );

		if( max_load-- <= 0 ) break;
	}
	closedir( artist_dir );

	return album_list_sort( album_list );
}

void change_playlist( Player *player, PlaylistManager *manager, int dir )
{
	LOG_DEBUG("player=p manager=p change_playlist", player, manager);
	PlaylistItem *x = player->current_track.playlist_item;
	if( !x ) {
		LOG_ERROR("no playlist item");
		return;
	}
	Playlist *p = x->parent;
	if( !p ) {
		LOG_ERROR("no parent playlist");
		return;
	}
	if( dir > 0 ) {
		p = p->next;
		if( p == NULL ) {
			p = manager->root;
		}
	} else if( dir < 0 ) {
		p = p->prev;
		if( p == NULL ) {
			p = manager->root;
			if( p ) {
				while( p->next ) {
					p = p->next;
				}
			}
		}
	}
	if( p ) {
		player_change_track( player, p->root, TRACK_CHANGE_IMMEDIATE );
	}
}

void* gpio_input_thread_run( void *p )
{
	int res;
	MyData *data = (MyData*) p;
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
		LOG_DEBUG("process GPIO input");

		data->player->playing = play_switch;
		if( rotation_switch ) {
			change_playlist( data->player, data->playlist_manager, rotation_switch );
		}
		rotation_switch = 0;
	}
	return NULL;
}


int main(int argc, char *argv[])
{
	int res;
	Player player;

	init_player( &player );

	AlbumList album_list;
	res = album_list_init( &album_list );
	if( res ) {
		LOG_ERROR("failed to init album list");
		return 1;
	}
	res = load_albums( &album_list, "/media/nugget_share/music/alex-beet", player.mh );
	if( res ) {
		LOG_ERROR("failed to load album list");
		return 1;
	}

	PlaylistManager playlist_manager;
	playlist_manager_init( &playlist_manager, "/home/alex/the_playlist.txt" );
	player.playlist_manager = &playlist_manager;

	LOG_DEBUG("calling load");
	res = playlist_manager_load( &playlist_manager );
	if( res ) {
		LOG_WARN("failed to load playlist");
	}
	LOG_DEBUG("load done");

	res = pthread_cond_init( &gpio_input_changed_cond, NULL );
	if( res ) {
		LOG_ERROR("failed to init gpio_input_changed_cond");
		return 1;
	}

	if( wiringPiSetup() == -1 ) {
		LOG_ERROR("failed to setup wiringPi");
		return 1;
	}

	// rotary encoders

	// See https://pinout.xyz/ for pinouts
	//
	// The following pins have a physical pull up resistor
	// we can't configure it via software, but in our case
	// we want a pull up resistor anyway as the rotary switch
	// will be connected to ground when the switch is closed
	pinMode( PIN_ROTARY_1, INPUT );
	pinMode( PIN_ROTARY_2, INPUT );

	// on/off switch
	pinMode( PIN_SWITCH, INPUT );
	pullUpDnControl( PIN_SWITCH, PUD_UP );
	int switch_current_pos = digitalRead( PIN_SWITCH );

	int pin1 = digitalRead( PIN_ROTARY_1 );
	int pin2 = digitalRead( PIN_ROTARY_2 );
	initRotaryState( &rotary_state, pin1, pin2 );

	wiringPiISR( PIN_ROTARY_1, INT_EDGE_BOTH, rotaryIntHandler);
	wiringPiISR( PIN_ROTARY_2, INT_EDGE_BOTH, rotaryIntHandler);
	wiringPiISR( PIN_SWITCH, INT_EDGE_BOTH, switchIntHandler);

	// get initial switch pos
	switchIntHandler();

	LCDState lcd_state;

	setupLCDPins(&lcd_state);

	writeText(&lcd_state, "Play me the hits");

	LOG_DEBUG("starting");

	res = start_player( &player );
	if( res ) {
		LOG_ERROR("failed to start player");
		return 1;
	}

	// TODO do this during the setup above
	// ideally split it into two funcs: init, and start threads
	player.playing = switch_current_pos;

	res = player_add_metadata_observer( &player, &update_metadata_lcd, (void *) &lcd_state );
	if( res ) {
		LOG_ERROR("failed to register observer");
		return 1;
	}

	MyData my_data = {
		&player,
		&album_list,
		&playlist_manager
	};

	res = pthread_create( &gpio_input_thread, NULL, &gpio_input_thread_run, (void*) &my_data );
	if( res ) {
		LOG_ERROR("failed to create input thread");
		return 1;
	}

	WebHandlerData web_handler_data;

	res = init_http_server_data( &web_handler_data, &album_list, &playlist_manager, &player );
	if( res ) {
		LOG_ERROR("failed to init http server");
		return 2;
	}

	res = player_add_metadata_observer( &player, &update_metadata_web_clients, (void*) &web_handler_data );
	if( res ) {
		LOG_ERROR("failed to register observer");
		return 1;
	}

	if( playlist_manager.root ) {
		PlaylistItem *x = playlist_manager.root->root;
		player_change_track( &player, x, TRACK_CHANGE_IMMEDIATE );
	}

	LOG_DEBUG("running server");
	res = start_http_server( &web_handler_data );
	if( res ) {
		LOG_ERROR("failed to start http server");
		return 2;
	}

	LOG_DEBUG("done");
	return 0;
}

void update_metadata_lcd(bool playing, const PlayerTrackInfo *track, void *data)
{
	char buf[1024];
	LCDState *lcd_state = (LCDState*) data;
	LOG_DEBUG( "artist=s title=s update_metadata_lcd", track->artist, track->title );
	if( playing ) {
		if( track->artist[0] ) {
			snprintf( buf, 1024, "%s - %s", track->artist, track->title );
		} else {
			// streams do not include artist; it is already pre-formatted in title
			if( track->title[0] ) {
				snprintf( buf, 1024, "%s", track->title );
			} else {
				snprintf( buf, 1024, "%s", track->icy_name );
			}
		}
		writeText( lcd_state, buf );
	} else {
		writeText( lcd_state, "paused" );
	}
}



//	mpg123_handle *mh;
//	unsigned char *buffer;
//	int copied = 0;
//	size_t buffer_size;
//	size_t done;
//	int err;
//
//	struct MHD_Daemon *d = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION,
//			80,
//			NULL,
//			NULL,
//			&web_handler,
//			NULL,
//			MHD_OPTION_END);
//	if( d == NULL ) {
//		printf("failed setting up wiringPi\n");
//		return 1;
//	}
//
//	//int driver;
//	//ao_device *dev;
//
//	//ao_sample_format format;
//	//int channels, encoding;
//	//long rate;
//
//	if( wiringPiSetup() == -1 ) {
//		printf("failed setting up wiringPi\n");
//		return 1;
//	}
//
//	// rotary encoders
//
//	// See https://pinout.xyz/ for pinouts
//	//
//	// The following pins have a physical pull up resistor
//	// we can't configure it via software, but in our case
//	// we want a pull up resistor anyway as the rotary switch
//	// will be connected to ground when the switch is closed
//	pinMode( PIN_ROTARY_1, INPUT );
//	pinMode( PIN_ROTARY_2, INPUT );
//
//	// on/off switch
//	pinMode( PIN_SWITCH, INPUT );
//	pullUpDnControl( PIN_SWITCH, PUD_UP );
//
//	int pin1 = digitalRead( PIN_ROTARY_1 );
//	int pin2 = digitalRead( PIN_ROTARY_2 );
//	initRotaryState( &rotary_state, pin1, pin2 );
//
//	wiringPiISR( PIN_ROTARY_1, INT_EDGE_BOTH, rotaryIntHandler);
//	wiringPiISR( PIN_ROTARY_2, INT_EDGE_BOTH, rotaryIntHandler);
//
//	wiringPiISR( PIN_SWITCH, INT_EDGE_BOTH, switchIntHandler);
//
//	// get initial switch pos
//	switchIntHandler();
//
//	LCDState lcd_state;
//
//	setupLCDPins(&lcd_state);
//
//	char playlist[1024][1024];
//	int playlist_items = 0;
//	int current_playlist_item;
//	int is_playlist_loaded = 0;
//	int pause_screen_cleared = 0;
//	while( 1 ) {
//		if( playing == 0 ) {
//			if( !pause_screen_cleared ) {
//				writeText(&lcd_state, "");
//				pause_screen_cleared = 1;
//			}
//
//			usleep(10000);
//			continue;
//		} else {
//			pause_screen_cleared = 0;
//		}
//		httpdata_reset(&hd);
//
//		// TODO re-use as much as possible
//		mh = mpg123_new(NULL, &err);
//		buffer_size = mpg123_outblock(mh);
//		buffer = (unsigned char*) malloc(buffer_size * sizeof(unsigned char));
//
//		const char *url = playlists[selected_playlist].url;
//		const char *playlist_name = playlists[selected_playlist].name;
//		playing_playlist = selected_playlist;
//
//
//		sprintf(text_buf, "loading: %s", playlist_name);
//		writeText(&lcd_state, text_buf);
//
//		int fd = -1;
//
//		if( strstr(url, "http://") ) {
//			fd = http_open(url, &hd);
//			printf("setting icy %ld\n", hd.icy_interval);
//			if(MPG123_OK != mpg123_param(mh, MPG123_ICY_INTERVAL, hd.icy_interval, 0)) {
//				printf("unable to set icy interval\n");
//				return 1;
//			}
//			if(mpg123_open_fd(mh, fd) != MPG123_OK) {
//				printf("error\n");
//				return 1;
//			}
//		} else {
//			if( !is_playlist_loaded ) {
//				playlist_items = 0;
//				current_playlist_item = 0;
//				FILE *fp = fopen(url, "r");
//				size_t len = 1024;
//				char *line = NULL;
//				while( getline(&line, &len, fp) != -1 ) {
//					for( int i = strlen(line) - 1; i >= 0; i-- ) {
//						if( line[i] == '\n' || line[i] == ' ' || line[i] == '\r' )
//						{
//							line[i] = '\0';
//						} else {
//break;
//}
//					}
//					strcpy(playlist[playlist_items], line);
//					printf("queueing %s\n", playlist[playlist_items]);
//					if( playlist_items > 1024 ) {
//						printf("too many songs in playlist\n");
//						break;
//					}
//					playlist_items++;
//				}
//				is_playlist_loaded = 1;
//				current_playlist_item = 0;
//			}
//			// TODO read playlist instead of treating it as a path to a mp3
//			/* open the file and get the decoding format */
//			printf("playing: %s\n", playlist[current_playlist_item]);
//			mpg123_open(mh, playlist[current_playlist_item]);
//			current_playlist_item = (current_playlist_item + 1) % playlist_items;
//		}
//		mpg123_getformat(mh, &rate, &channels, &encoding);
//
//		/* set the output format and open the output device */
//		format.bits = mpg123_encsize(encoding) * BITS;
//		format.rate = rate;
//		format.channels = channels;
//		format.byte_format = AO_FMT_NATIVE;
//		format.matrix = 0;
//		dev = ao_open_live(driver, &format, NULL);
//
//		mpg123_id3v1 *v1;
//		mpg123_id3v2 *v2;
//		char *icy_meta;
//
//		/* decode and play */
//		while (mpg123_read(mh, buffer, buffer_size, &done) == MPG123_OK) {
//			int meta = mpg123_meta_check(mh);
//			if( meta & MPG123_NEW_ID3 ) {
//				if( mpg123_id3(mh, &v1, &v2) == MPG123_OK ) {
//					//printf("got meta\n");
//					if( v1 != NULL ) {
//						printf("v1 title: %s\n", v1->title);
//						printf("v1 artist: %s\n", v1->artist);
//						sprintf(text_buf, "%s: %s - %s", playlist_name, v1->artist, v1->title);
//						printf("v1 text: %s\n", text_buf);
//						writeText(&lcd_state, text_buf);
//					}
//					if( v2 != NULL ) {
//						printf("v2 title: %s\n", v2->title->p);
//						printf("v2 artist: %s\n", v2->artist->p);
//						sprintf(text_buf, "%s: %s - %s", playlist_name, v2->artist->p, v2->title->p);
//						printf("v2 text: %s\n", text_buf);
//						writeText(&lcd_state, text_buf);
//					}
//				}
//			}
//			if( meta & MPG123_NEW_ICY ) {
//				if( mpg123_icy(mh, &icy_meta) == MPG123_OK ) {
//					printf("got ICY: %s\n", icy_meta);
//					parseICY(icy_meta, playlist_name, text_buf);
//					writeText(&lcd_state, text_buf);
//
//				}
//			}
//
//			ao_play(dev, (char *) buffer, done);
//
//			if( !playing || selected_playlist != playing_playlist )
//				break;
//		}
//
//		mpg123_close(mh);
//		mpg123_delete(mh);
//		if( fd >= 0 ) {
//			close(fd);
//			fd = -1;
//		}
//
//		free(buffer);
//		ao_close(dev);
//	}
//
//	/* clean up */
//	mpg123_exit();
//	ao_shutdown();
//
//	return 0;
//}
