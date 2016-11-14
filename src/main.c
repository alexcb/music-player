#include <ao/ao.h>
#include <mpg123.h>
#include <string.h>
#include <wiringPi.h>
#include <assert.h>
#include <font8x8_basic.h>
#include <unistd.h>


#include "httpget.h"
#include "lcd.h"
#include "rotary.h"

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

typedef struct Playlist
{
	const char *name;
	const char *url;
} Playlist;
Playlist playlists[] = {
	{
		"kexp",
		"http://live-mp3-128.kexp.org:80/kexp128.mp3"
	},
	{
		"kcrw",
		"http://kcrw.streamguys1.com/kcrw_192k_mp3_e24_internet_radio"
	},
	{
		"witr",
		"http://streaming.witr.rit.edu:8000/witr-undrgnd-mp3-192"
	},
	{
		"mp3 list",
		"/home/alex/foo.m3u"
	}
};

// can be changed in interupt handlers, must be volitile
volatile int playing_playlist = 0;
volatile int selected_playlist = 3;
volatile int num_playlists = 4;
volatile int playing = 0;

RotaryState rotary_state;

void rotaryIntHandler()
{
	int pin1 = digitalRead( PIN_ROTARY_1 );
	int pin2 = digitalRead( PIN_ROTARY_2 );
	int res = updateRotary(&rotary_state, pin1, pin2);
	if( res > 0 ) {
		selected_playlist = (selected_playlist + 1) % num_playlists;
		printf( "clockwise\n" );
	} else if( res < 0 ) {
		selected_playlist = (selected_playlist + num_playlists - 1) % num_playlists;
		printf( "counter-clockwise\n" );
	}
}

void switchIntHandler()
{
	playing = digitalRead( PIN_SWITCH );
	printf( "switch: %d\n", playing);
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

		if( lcd_offset < 504 ) {
			img_buffer[lcd_offset] = img_buffer[lcd_offset] | lcd_mask;
		} else {
printf("OVERFLOW\n");
	}
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

void parseICY( const char *icy_meta, const char *playlist_name, char *text_buf )
{
	const char *streamTitlePrefix = "StreamTitle='";
	char *p = strstr(icy_meta, streamTitlePrefix);
	if( !p ) {
		snprintf(text_buf, 1024, "%s: UNKNOWN", playlist_name);
		return;
	}
	p += strlen(streamTitlePrefix);

	char *q = strstr(icy_meta, "';");
	if( !q ) {
		snprintf(text_buf, 1024, "%s: %s", playlist_name, p);
		return;
	}
	size_t len = q - p;

	snprintf(text_buf, 1024, "%s: ", playlist_name);

	char *s = text_buf + strlen(text_buf);
	memcpy(s, p, len);
	s[len+1] = '\0';
}


int main(int argc, char *argv[])
{
	mpg123_handle *mh;
	unsigned char *buffer;
	int copied = 0;
	size_t buffer_size;
	size_t done;
	int err;

	int driver;
	ao_device *dev;

	ao_sample_format format;
	int channels, encoding;
	long rate;

	if( wiringPiSetup() == -1 ) {
		printf("failed setting up wiringPi\n");
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

	/* initializations */
	ao_initialize();
	driver = ao_default_driver_id();
	mpg123_init();


	char playlist[1024][1024];
	int playlist_items = 0;
	int current_playlist_item;
	int is_playlist_loaded = 0;
	int pause_screen_cleared = 0;
	while( 1 ) {
		if( playing == 0 ) {
			if( !pause_screen_cleared ) {
				writeText(&lcd_state, "");
				pause_screen_cleared = 1;
			}

			usleep(10000);
			continue;
		} else {
			pause_screen_cleared = 0;
		}
		httpdata_reset(&hd);

		// TODO re-use as much as possible
		mh = mpg123_new(NULL, &err);
		buffer_size = mpg123_outblock(mh);
		buffer = (unsigned char*) malloc(buffer_size * sizeof(unsigned char));

		const char *url = playlists[selected_playlist].url;
		const char *playlist_name = playlists[selected_playlist].name;
		playing_playlist = selected_playlist;


		sprintf(text_buf, "loading: %s", playlist_name);
		writeText(&lcd_state, text_buf);

		int fd = -1;

		if( strstr(url, "http://") ) {
			fd = http_open(url, &hd);
			printf("setting icy %ld\n", hd.icy_interval);
			if(MPG123_OK != mpg123_param(mh, MPG123_ICY_INTERVAL, hd.icy_interval, 0)) {
				printf("unable to set icy interval\n");
				return 1;
			}
			if(mpg123_open_fd(mh, fd) != MPG123_OK) {
				printf("error\n");
				return 1;
			}
		} else {
			if( !is_playlist_loaded ) {
				playlist_items = 0;
				current_playlist_item = 0;
				FILE *fp = fopen(url, "r");
				size_t len = 1024;
				char *line = NULL;
				while( getline(&line, &len, fp) != -1 ) {
					for( int i = strlen(line) - 1; i >= 0; i-- ) {
						if( line[i] == '\n' || line[i] == ' ' || line[i] == '\r' )
						{
							line[i] = '\0';
						} else {
break;
}
					}
					strcpy(playlist[playlist_items], line);
					printf("queueing %s\n", playlist[playlist_items]);
					if( playlist_items > 1024 ) {
						printf("too many songs in playlist\n");
						break;
					}
					playlist_items++;
				}
				is_playlist_loaded = 1;
				current_playlist_item = 0;
			}
			// TODO read playlist instead of treating it as a path to a mp3
			/* open the file and get the decoding format */
			printf("playing: %s\n", playlist[current_playlist_item]);
			mpg123_open(mh, playlist[current_playlist_item]);
			current_playlist_item = (current_playlist_item + 1) % playlist_items;
		}
		mpg123_getformat(mh, &rate, &channels, &encoding);

		/* set the output format and open the output device */
		format.bits = mpg123_encsize(encoding) * BITS;
		format.rate = rate;
		format.channels = channels;
		format.byte_format = AO_FMT_NATIVE;
		format.matrix = 0;
		dev = ao_open_live(driver, &format, NULL);

		mpg123_id3v1 *v1;
		mpg123_id3v2 *v2;
		char *icy_meta;

		/* decode and play */
		while (mpg123_read(mh, buffer, buffer_size, &done) == MPG123_OK) {
			int meta = mpg123_meta_check(mh);
			if( meta & MPG123_NEW_ID3 ) {
				if( mpg123_id3(mh, &v1, &v2) == MPG123_OK ) {
					//printf("got meta\n");
					if( v1 != NULL ) {
						printf("v1 title: %s\n", v1->title);
						printf("v1 artist: %s\n", v1->artist);
						sprintf(text_buf, "%s: %s - %s", playlist_name, v1->artist, v1->title);
						printf("v1 text: %s\n", text_buf);
						writeText(&lcd_state, text_buf);
					}
					if( v2 != NULL ) {
						printf("v2 title: %s\n", v2->title->p);
						printf("v2 artist: %s\n", v2->artist->p);
						sprintf(text_buf, "%s: %s - %s", playlist_name, v2->artist->p, v2->title->p);
						printf("v2 text: %s\n", text_buf);
						writeText(&lcd_state, text_buf);
					}
				}
			}
			if( meta & MPG123_NEW_ICY ) {
				if( mpg123_icy(mh, &icy_meta) == MPG123_OK ) {
					printf("got ICY: %s\n", icy_meta);
					parseICY(icy_meta, playlist_name, text_buf);
					writeText(&lcd_state, text_buf);

				}
			}

			ao_play(dev, (char *) buffer, done);

			if( !playing || selected_playlist != playing_playlist )
				break;
		}

		mpg123_close(mh);
		mpg123_delete(mh);
		if( fd >= 0 ) {
			close(fd);
			fd = -1;
		}

		free(buffer);
		ao_close(dev);
	}

	/* clean up */
	mpg123_exit();
	ao_shutdown();

	return 0;
}
