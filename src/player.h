#pragma once

#include <ao/ao.h>
#include <mpg123.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

#include "circular_buffer.h"
#include "play_queue.h"
#include "playlist_manager.h"
#include "httpget.h"

#define PLAYER_ARTIST_LEN 1024
#define PLAYER_TITLE_LEN 1024

#define TRACK_CHANGE_IMMEDIATE 1
#define TRACK_CHANGE_NEXT 2

typedef struct PlayerTrackInfo {
	char artist[PLAYER_ARTIST_LEN];
	char title[PLAYER_TITLE_LEN];
	PlaylistItem *playlist_item;
	bool is_stream;
	char icy_name[PLAYER_TITLE_LEN];
} PlayerTrackInfo;

typedef void (*MetadataObserver)(bool playing, const PlayerTrackInfo *track, void *data);

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

	int reading_index;
	int playing_index;
	int track_change_mode;
	volatile bool next_track;

	pthread_t audio_thread;
	pthread_t reader_thread;

	CircularBuffer circular_buffer;

	PlayQueue play_queue;
	pthread_mutex_t play_queue_lock;

	struct httpdata hd;

	PlaylistManager *playlist_manager;

	int metadata_observers_num;
	int metadata_observers_cap;
	MetadataObserver *metadata_observers;
	void **metadata_observers_data;

	PlayerTrackInfo current_track;

	// when true play, when false, pause / stop
	volatile bool playing;

	int reading_playlist_id;
	int reading_playlist_track;
	
	// control over changing tracks
	pthread_mutex_t change_track_lock;
	int change_track; // 0 when false; 1 insert after current song; 2 change immediately
	PlaylistItem *change_playlist_item;
	
	char *audio_thread_p[2];
	size_t audio_thread_size[2];

	size_t max_payload_size;

	size_t decode_buffer_size;
	char *decode_buffer;
} Player;


int init_player( Player *player );
int start_player( Player *player );

int player_add_metadata_observer( Player *player, MetadataObserver observer, void *data );

int player_change_track( Player *player, PlaylistItem *playlist_item, int when );
int player_reload_next_track( Player *player );
int player_notify_item_change( Player *player, PlaylistItem *playlist_item );

void player_set_playing( Player *player, bool playing );
