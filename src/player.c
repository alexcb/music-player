#include "player.h"

#include <string.h>

#define BITS 8

void player_thread_run( void *data );

int start_player( Player *player )
{
	int res = pthread_create( &player->thread, NULL, (void *) &player_thread_run, (void *) player);
	if( !res ) {
		goto error;
	}

	return 0;

error:
	//free( player->buffer );
	//mpg123_exit();
	//ao_shutdown();
	return 1;
}

void player_thread_run( void *data )
{
	Player *player = (Player*) data;

	int driver;
	ao_device *dev;
	ao_sample_format format;
	int channels, encoding;
	long rate;

	mpg123_handle *mh;
	unsigned char *buffer;
	int copied;
	size_t buffer_size;
	size_t done;
	int err;

	pthread_t thread;

	struct httpdata hd;

	ao_initialize();
	driver = ao_default_driver_id();
	mpg123_init();


	mh = mpg123_new( NULL, &err );
	buffer_size = mpg123_outblock( mh );
	buffer = (unsigned char*) malloc( buffer_size );

	int fd = -1;

	const char *url = "http://streaming.witr.rit.edu:8000/witr-undrgnd-mp3-192";

	if( strstr(url, "http://") ) {
		fd = http_open(url, &hd);
		printf("setting icy %ld\n", hd.icy_interval);
		if(MPG123_OK != mpg123_param(mh, MPG123_ICY_INTERVAL, hd.icy_interval, 0)) {
			printf("unable to set icy interval\n");
			return;
		}
		if(mpg123_open_fd(mh, fd) != MPG123_OK) {
			printf("error\n");
			return;
		}
	} else {
		//if( !is_playlist_loaded ) {
		//	playlist_items = 0;
		//	current_playlist_item = 0;
		//	FILE *fp = fopen(url, "r");
		//	size_t len = 1024;
		//	char *line = NULL;
		//	while( getline(&line, &len, fp) != -1 ) {
		//		for( int i = strlen(line) - 1; i >= 0; i-- ) {
		//			if( line[i] == '\n' || line[i] == ' ' || line[i] == '\r' )
		//			{
		//				line[i] = '\0';
		//			} else {
		//				break;
		//			}
		//		}
		//		strcpy(playlist[playlist_items], line);
		//		printf("queueing %s\n", playlist[playlist_items]);
		//		if( playlist_items > 1024 ) {
		//			printf("too many songs in playlist\n");
		//			break;
		//		}
		//		playlist_items++;
		//	}
		//	is_playlist_loaded = 1;
		//	current_playlist_item = 0;
		//}
		//// TODO read playlist instead of treating it as a path to a mp3
		///* open the file and get the decoding format */
		//printf("playing: %s\n", playlist[current_playlist_item]);
		//mpg123_open(mh, playlist[current_playlist_item]);
		//current_playlist_item = (current_playlist_item + 1) % playlist_items;
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
				//if( v1 != NULL ) {
				//	printf("v1 title: %s\n", v1->title);
				//	printf("v1 artist: %s\n", v1->artist);
				//	sprintf(text_buf, "%s: %s - %s", playlist_name, v1->artist, v1->title);
				//	printf("v1 text: %s\n", text_buf);
				//	writeText(&lcd_state, text_buf);
				//}
				//if( v2 != NULL ) {
				//	printf("v2 title: %s\n", v2->title->p);
				//	printf("v2 artist: %s\n", v2->artist->p);
				//	sprintf(text_buf, "%s: %s - %s", playlist_name, v2->artist->p, v2->title->p);
				//	printf("v2 text: %s\n", text_buf);
				//	writeText(&lcd_state, text_buf);
				//}
			}
		}
		if( meta & MPG123_NEW_ICY ) {
			if( mpg123_icy(mh, &icy_meta) == MPG123_OK ) {
				//printf("got ICY: %s\n", icy_meta);
				//parseICY(icy_meta, playlist_name, text_buf);
				//writeText(&lcd_state, text_buf);

			}
		}

		ao_play(dev, (char *) buffer, done);

		//if( !playing || selected_playlist != playing_playlist )
		//	break;
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



int stop_player( Player *player )
{
	mpg123_exit();
	ao_shutdown();
}
