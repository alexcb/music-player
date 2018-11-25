#pragma once

#include "player.h"
#include "library.h"
#include "playlist_manager.h"
#include "streams.h"
#include "sds.h"

// TODO rename this (and this file)
typedef struct MyData {
	Player *player;
	Library *library;
	PlaylistManager *playlist_manager;
	StreamList *stream_list;
	sds library_json;
} MyData;

