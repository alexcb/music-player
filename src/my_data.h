#pragma once

#include "player.h"
#include "albums.h"
#include "playlist_manager.h"
#include "streams.h"
#include "sds.h"

// TODO rename this (and this file)
typedef struct MyData {
	Player *player;
	AlbumList *album_list;
	PlaylistManager *playlist_manager;
	StreamList *stream_list;
	sds library_json;
} MyData;

