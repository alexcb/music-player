#include <ao/ao.h>
#include <mpg123.h>
#include <unistd.h>
#include <pthread.h>

#include "playlist.h"
#include "httpget.h"

typedef struct Player
{
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

	Playlist *playlist;


} Player;


int start_player( Player *player );

int set_playlist( Player *player, Playlist *playlist );
