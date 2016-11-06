#include <ao/ao.h>
#include <mpg123.h>
#include <string.h>

#include "httpget.h"

#define BITS 8

struct httpdata hd;

int main(int argc, char *argv[])
{
	mpg123_handle *mh;
	unsigned char *buffer;
	size_t buffer_size;
	size_t done;
	int err;

	int driver;
	ao_device *dev;

	ao_sample_format format;
	int channels, encoding;
	long rate;

	if(argc < 2)
		exit(0);

	/* initializations */
	ao_initialize();
	driver = ao_default_driver_id();
	mpg123_init();
	mh = mpg123_new(NULL, &err);
	buffer_size = mpg123_outblock(mh);
	buffer = (unsigned char*) malloc(buffer_size * sizeof(unsigned char));

	httpdata_reset(&hd);

	if( strstr(argv[1], "http://") ) {
		int fd = http_open(argv[1], &hd);
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
		/* open the file and get the decoding format */
		mpg123_open(mh, argv[1]);
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
				printf("got meta\n");
				if( v1 != NULL ) {
					printf("title: %s\n", v1->title);
					printf("artist: %s\n", v1->artist);
				}
				if( v2 != NULL ) {
					printf("title: %s\n", v2->title->p);
					printf("artist: %s\n", v2->artist->p);
				}
			}
		}
		if( meta & MPG123_NEW_ICY ) {
			if( mpg123_icy(mh, &icy_meta) == MPG123_OK ) {
				printf("got ICY: %s\n", icy_meta);

			}
		}

		ao_play(dev, (char *) buffer, done);
	}

	/* clean up */
	free(buffer);
	ao_close(dev);
	mpg123_close(mh);
	mpg123_delete(mh);
	mpg123_exit();
	ao_shutdown();

	return 0;
}
