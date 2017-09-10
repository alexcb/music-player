#include <ao/ao.h>
#include <mpg123.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>

#include <dirent.h>


#include "httpget.h"
#include "player.h"
#include "albums.h"
#include "playlist_manager.h"
#include "playlist.h"
#include "string_utils.h"
#include "log.h"
#include "errors.h"
#include "my_data.h"
#include "web.h"
#include "streams.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "id3.h"


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

struct httpdata hd;

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
		PlaylistItem *x = p->root;
		if( p->current && p->current->next ) {
			x = p->current->next;
		}
		player_change_track( player, x, TRACK_CHANGE_IMMEDIATE );
	}
}


// limit should be -1; only positive value is used for testing to speed up load times
void find_tracks( ID3Cache *cache, const char *path, int *limit )
{
	if( limit && *limit == 0 ) { return; }
	struct dirent *dirent;

	sds s = sdsnew(path);

	LOG_DEBUG( "path=s opening dir", path );
	DIR *dir = opendir(path);
	if( dir == NULL ) {
		LOG_ERROR( "path=s err=s opendir failed", path, strerror(errno) );
		sdsfree(s);
		return;
	}

	while( (dirent = readdir(dir)) != NULL ) {
		if( limit ) {
			if( *limit == 0 ) break;
			(*limit)--;
		}
		LOG_DEBUG( "path=s readdir", dirent->d_name );
		if( strcmp(dirent->d_name, ".") == 0 || strcmp(dirent->d_name, "..") == 0 ) {
			continue;
		}

		sdsclear( s );
		s = sdscatfmt( s, "%s/%s", path, dirent->d_name );

		if( dirent->d_type == DT_DIR ) {
			find_tracks( cache, s, limit );
			continue;
		}

		if( has_suffix( s, ".mp3" ) ) {
			LOG_DEBUG( "path=s opening file", s );
			id3_cache_add( cache, s );
		}
	}
	closedir( dir );
}

int main(int argc, char *argv[])
{
	int res;
	Player player;
	ID3Cache *cache;

	if( argc != 4 ) {
		printf("%s <albums path> <streams path> <playlist path>\n", argv[0]);
		return 1;
	}
	char *music_path = argv[1];
	char *streams_path = argv[2];
	char *playlist_path = argv[3];


	init_player( &player );

	res = id3_cache_new( &cache, "/tmp/id3_cache", player.mh );
	if( res ) {
		LOG_ERROR("failed to load cache");
		return 1;
	}

	StreamList stream_list;
	stream_list.p = NULL;
	res = parse_streams( streams_path, &stream_list );
	if( res ) {
		LOG_ERROR("failed to load streams");
		return 1;
	}


	// This pre-populates the id3 cache
	// this does not create the album list
	// this code will be removed, and caching can simply be done while reading albums
	if (0) {
		int limit = 100;
		find_tracks( cache, music_path, &limit );
		LOG_INFO("saving cache");
		id3_cache_save( cache );
		return 0;
	}

	AlbumList album_list;
	res = album_list_init( &album_list, cache );
	if( res ) {
		LOG_ERROR("err=d failed to init album list", res);
		return 1;
	}
	int albumlimit = 5;
	res = album_list_load( &album_list, music_path, &albumlimit );
	if( res ) {
		LOG_ERROR("err=d failed to load albums", res);
		return 1;
	}
	
	PlaylistManager playlist_manager;
	playlist_manager_init( &playlist_manager, playlist_path );
	player.playlist_manager = &playlist_manager;

	LOG_DEBUG("calling load");
	res = playlist_manager_load( &playlist_manager );
	if( res ) {
		LOG_WARN("failed to load playlist");
	}
	LOG_DEBUG("load done");

	LOG_DEBUG("starting");

	res = start_player( &player );
	if( res ) {
		LOG_ERROR("failed to start player");
		return 1;
	}

	MyData my_data = {
		&player,
		&album_list,
		&playlist_manager,
		&stream_list
	};

	WebHandlerData web_handler_data;

	res = init_http_server_data( &web_handler_data, &my_data );
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
