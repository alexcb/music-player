#pragma once

#include <ao/ao.h>
#include <mpg123.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

#include "playlist_manager.h"
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

	PlaylistManager *playlist_manager;

	// when true play, when false, pause / stop
	bool playing;

	// set to true when player should
	// query playlist for which file to play (used when changing tracks)
	bool restart;




} Player;


int start_player( Player *player );

