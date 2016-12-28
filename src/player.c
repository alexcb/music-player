#include "player.h"

#include <string.h>
#include <math.h>

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <unistd.h>
#include <err.h>
#include <assert.h>

#include <ao/ao.h>


#define BITS 8

void player_thread_run( void *data );

int start_player( Player *player )
{
	int res;

	ao_initialize();
	player->driver = ao_default_driver_id();

	player->format.bits = 16;
	player->format.rate = 44100;
	player->format.channels = 2;
	player->format.byte_format = AO_FMT_NATIVE;
	player->format.matrix = NULL;

	player->dev = ao_open_live( player->driver, &player->format, NULL );

	mpg123_init();
	player->mh = mpg123_new( NULL, NULL );

	player->playing = true;
	player->restart = false;

	//player->playlist = NULL;
	//res = playlist_new( &player->playlist, "test playlist" );
	//if( res ) {
	//	goto error;
	//}

	//res = playlist_add_file( player->playlist, "/home/alex/test1.mp3" );
	//if( res ) {
	//	goto error;
	//}

	//res = playlist_add_file( player->playlist, "/home/alex/test2.mp3" );
	//if( res ) {
	//	goto error;
	//}

	//res = playlist_add_file( player->playlist, "/home/alex/test3.mp3" );
	//if( res ) {
	//	goto error;
	//}

	printf("about to create thread\n");
	res = pthread_create( &player->thread, NULL, (void *) &player_thread_run, (void *) player);
	if( res ) {
		goto error;
	}

	return 0;

error:
	//free( player->buffer );
	//mpg123_exit();
	//ao_shutdown();
	printf("failed starting player\n");
	return 1;
}

//int set_playlist( Player *player, Playlist *playlist )
//{
//	player->playlist = playlist;
//	player->restart = true;
//}

void play_tone( Player *player )
{
	size_t bufsize = player->format.rate * player->format.channels;
	short *buf = (short*) malloc(bufsize * sizeof(short));
	float a = 0.0;
	float b = 0.0;
	float t = (M_PI*2) *  400.0 / player->format.rate;
	float t2 = (M_PI*2) * 312.0 / player->format.rate;
	int i;
	for (i = 0; i < player->format.rate; ++i) {
		buf[i] = SHRT_MAX * (sin(a)/2.0 + sin(b)/2.0);
		a += t;
		b += t2;
		while (a >= M_PI * 2)
			a -= M_PI * 2;
		while (b >= M_PI * 2)
			b -= M_PI * 2;
	}

	for ( ;; ) {
		if (ao_play(player->dev, (char*) buf, bufsize) == -1) {
			break;
			//errx(1, "ao_play");
		}
	}
}

void player_thread_run( void *data )
{
	int res;
	Player *player = (Player*) data;

	//play_tone( player );
	//setbuf(stdout, NULL);

	printf("player running\n");
	for(;;) {
		if( !player->playing ) {
			printf("paused\n");
			sleep(1);
			continue;
		}
		//if( player->playlist == NULL ) {
		//	printf("no playlist\n");
		//	sleep(1);
		//	continue;
		//}
		int fd;
		res = playlist_manager_open_fd( player->playlist_manager, &fd );
		if( res ) {
			printf("no fd returned\n");
			sleep(1);
			continue;
		}
		printf("opening %d\n", fd);

		if( mpg123_open_fd( player->mh, fd ) != MPG123_OK ) {
			printf("failed to open\n");
			close( fd );
			continue;
		}

		size_t buffer_size = mpg123_outblock( player->mh );
		unsigned char *buffer = (unsigned char*) malloc(buffer_size);

		size_t decoded_size;
		printf("decoding\n");
		bool done = false;
		while( !done ) {
			if( !player->playing ) {
				// TODO might need to close off internet streams instead of pausing
				// depending on what type of fd we are reading from
				usleep(100);
				continue;
			}
			if( player->restart ) {
				//usleep(100);
				player->restart = false;
				break;
			}
			res = mpg123_read( player->mh, buffer, buffer_size, &decoded_size);
			switch( res ) {
				case MPG123_OK:
					ao_play( player->dev, buffer, decoded_size );
					continue;
				case MPG123_NEW_FORMAT:
					printf("TODO handle new format\n");
					break;
				case MPG123_DONE:
					done = true;
					break;
				default:
					printf("non-handled -- mpg123_read returned: %s\n", mpg123_plain_strerror(res));
					break;
			}
		}
		close( fd );
	}

//
//	ao_device *dev;
//	ao_sample_format format;
//	int channels, encoding;
//	long rate;
//
//	mpg123_handle *mh;
//	unsigned char *buffer;
//	int copied;
//	size_t buffer_size;
//	size_t done;
//	int err;
//
//	pthread_t thread;
//
//	struct httpdata hd;
//
//
//
//	mh = mpg123_new( NULL, &err );
//	buffer_size = mpg123_outblock( mh );
//	buffer = (unsigned char*) malloc( buffer_size );
//
//	int fd = -1;
//
//	const char *url = "http://streaming.witr.rit.edu:8000/witr-undrgnd-mp3-192";
//
//	if( strstr(url, "http://") ) {
//		fd = http_open(url, &hd);
//		printf("setting icy %ld\n", hd.icy_interval);
//		printf("here5\n");
//		if(MPG123_OK != mpg123_param(mh, MPG123_ICY_INTERVAL, hd.icy_interval, 0)) {
//			printf("unable to set icy interval\n");
//			return;
//		}
//		if(mpg123_open_fd(mh, fd) != MPG123_OK) {
//			printf("error\n");
//			return;
//		}
//	}
//	printf("here6\n");
//	mpg123_getformat(mh, &rate, &channels, &encoding);
//
//	/* set the output format and open the output device */
//	format.bits = mpg123_encsize(encoding) * BITS;
//	format.rate = rate;
//	format.channels = channels;
//	format.byte_format = AO_FMT_NATIVE;
//	format.matrix = 0;
//	printf("here7\n");
//	dev = ao_open_live(player->driver, &format, NULL);
//
//	mpg123_id3v1 *v1;
//	mpg123_id3v2 *v2;
//	char *icy_meta;
//
//	/* decode and play */
//	printf("here8\n");
//	while (mpg123_read(mh, buffer, buffer_size, &done) == MPG123_OK) {
//		int meta = mpg123_meta_check(mh);
//		if( meta & MPG123_NEW_ID3 ) {
//			if( mpg123_id3(mh, &v1, &v2) == MPG123_OK ) {
//				//printf("got meta\n");
//				//if( v1 != NULL ) {
//				//	printf("v1 title: %s\n", v1->title);
//				//	printf("v1 artist: %s\n", v1->artist);
//				//	sprintf(text_buf, "%s: %s - %s", playlist_name, v1->artist, v1->title);
//				//	printf("v1 text: %s\n", text_buf);
//				//	writeText(&lcd_state, text_buf);
//				//}
//				//if( v2 != NULL ) {
//				//	printf("v2 title: %s\n", v2->title->p);
//				//	printf("v2 artist: %s\n", v2->artist->p);
//				//	sprintf(text_buf, "%s: %s - %s", playlist_name, v2->artist->p, v2->title->p);
//				//	printf("v2 text: %s\n", text_buf);
//				//	writeText(&lcd_state, text_buf);
//				//}
//			}
//		}
//		if( meta & MPG123_NEW_ICY ) {
//			if( mpg123_icy(mh, &icy_meta) == MPG123_OK ) {
//				//printf("got ICY: %s\n", icy_meta);
//				//parseICY(icy_meta, playlist_name, text_buf);
//				//writeText(&lcd_state, text_buf);
//
//			}
//		}
//
//		ao_play(dev, (char *) buffer, done);
//
//		//if( !playing || selected_playlist != playing_playlist )
//		//	break;
//	}
//
//	mpg123_close(mh);
//	mpg123_delete(mh);
//	if( fd >= 0 ) {
//		close(fd);
//		fd = -1;
//	}
//
//	free(buffer);
//	ao_close(dev);
}



int stop_player( Player *player )
{
	mpg123_exit();
	ao_shutdown();
}
