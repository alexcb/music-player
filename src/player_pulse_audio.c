#ifndef USE_RASP_PI

#include "player.h"
#include "log.h"
#include "io_utils.h"

#include <string.h>
#include <math.h>

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <unistd.h>
#include <err.h>
#include <assert.h>

#define RATE 44100


void init_pa( Player *player )
{
    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_S16LE,
        .rate = RATE,
        .channels = 2
    };

	int error;
	player->pa_handle = pa_simple_new(NULL, "alexplayer", PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, NULL, &error);
    if( player->pa_handle == NULL ) {
        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
		assert( 0 );
    }
}

int consume_pa( Player *player, const char *p, size_t n )
{
	int error;
	int res;
	res = pa_simple_write( player->pa_handle, p, n, &error);
	if( res < 0 ) {
		LOG_ERROR("res=d err=d pa_simple_write failed", res, error);
	}
	return 0;	
}

#endif
