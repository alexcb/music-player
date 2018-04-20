#ifdef USE_RASP_PI

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


int xrun_recovery(snd_pcm_t *handle, int err)
{
	if (err == -EPIPE) {    /* under-run */
		err = snd_pcm_prepare(handle);
		if (err < 0) {
			LOG_ERROR("err=s Can't recovery from underrun, prepare failed", snd_strerror(err));
		}
		return 0;
	} else if (err == -ESTRPIPE) {
		while ((err = snd_pcm_resume(handle)) == -EAGAIN) {
			sleep(1);       /* wait until the suspend flag is released */
		}
		if (err < 0) {
			err = snd_pcm_prepare(handle);
			if (err < 0) {
				LOG_ERROR("err=s Can't recovery from suspend, prepare failed", snd_strerror(err));
			}
		}
		return 0;
	}
	return err;
}

snd_pcm_t* open_sound_dev()
{
	int res;
	const char *device = "plughw:0,0";
	unsigned int period, bps;
	snd_pcm_t *pcm_handle;
	snd_pcm_hw_params_t *params;
	snd_pcm_uframes_t frames;

	unsigned int rate = RATE;
	unsigned int channels = 2;

	/* Open the PCM device in playback mode */
	res = snd_pcm_open(&pcm_handle, device, SND_PCM_STREAM_PLAYBACK, 0);
	if (res < 0) {
		LOG_ERROR( "err=s dev=s Can't open PCM device", snd_strerror(res), device );
	}

	/* Allocate parameters object and fill it with default values*/
	snd_pcm_hw_params_alloca(&params);

	snd_pcm_hw_params_any(pcm_handle, params);

	/* Set parameters */
	res = snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if( res < 0 ) {
		LOG_ERROR("err=s Can't set interleaved mode", snd_strerror(res));
		exit(EXIT_FAILURE);
	}

	res = snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE);
	if( res < 0 ) {
		LOG_ERROR("err=s Can't set format", snd_strerror(res));
		exit(EXIT_FAILURE);
	}

	res = snd_pcm_hw_params_set_channels(pcm_handle, params, channels);
	if( res < 0 ) {
		LOG_ERROR("err=s Can't set channels number", snd_strerror(res));
		exit(EXIT_FAILURE);
	}

	res = snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, 0);
	if( res < 0 ) {
		LOG_ERROR("err=s Can't set rate", snd_strerror(res));
		exit(EXIT_FAILURE);
	}
	if( rate != RATE ) {
		LOG_ERROR("rate=d failed to set rate to exact value", rate);
		exit(EXIT_FAILURE);
	}

	res = snd_pcm_hw_params_set_buffer_size(pcm_handle, params, 1024*4);
	if( res < 0 ) {
		LOG_ERROR("err=s Can't set buffersize", snd_strerror(res));
		exit(EXIT_FAILURE);
	}

	/* Write parameters */
	res = snd_pcm_hw_params(pcm_handle, params);
	if( res < 0 ) {
		LOG_ERROR("err=s Can't set harware parameters", snd_strerror(res));
		exit(EXIT_FAILURE);
	}

	snd_pcm_hw_params_get_channels(params, &channels);
	snd_pcm_hw_params_get_rate(params, &bps, 0);
	snd_pcm_hw_params_get_period_size(params, &frames, 0);
	snd_pcm_hw_params_get_period_time(params, &period, NULL);

	/* Resume information */
	LOG_INFO("name=s state=s channels=d bps=d frames=d period=d rate=d pcm info",
		snd_pcm_name(pcm_handle), snd_pcm_state_name(snd_pcm_state(pcm_handle)), channels, bps, frames, period, rate);

	return pcm_handle;
}

void init_alsa( Player *player )
{
	if( player->alsa_handle != NULL ) {
		//snd_pcm_drain(pcm_handle);
		snd_pcm_close( player->alsa_handle );
	}
	player->alsa_handle = open_sound_dev();
}

int consume_alsa( Player *player, const char *p, size_t n )
{
	int res;
	signed short *ptr = (signed short*) p;
	int frames = n / (2 * 2); //channels*16bits
	while( frames > 0 ) {
		res = snd_pcm_writei(player->alsa_handle, ptr, frames);
		if (res == -EAGAIN) {
			LOG_DEBUG("got EAGAIN");
			continue;
		} else if (res < 0) {
			if (xrun_recovery(player->alsa_handle, res) < 0) {
				LOG_ERROR("err=s write error", snd_strerror(res));
				//exit(EXIT_FAILURE);
				// TODO instead of exiting, try to re-init sound driver?
				// I've seen this before:
				//   - Input/output error

				// re-init sound driver
				sleep(1);
				init_alsa( player );
			} else {
				LOG_WARN("underrun");
			}
			break;  /* skip chunk -- there was a recoverable error */
		} else if( res == 0 ) {
			usleep( 1000 );
		} else {
			ptr += res;
			frames -= res;
		}
	}
	return 0;
}

#endif
