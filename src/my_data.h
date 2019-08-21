#pragma once

#include "library.h"
#include "player.h"
#include "playlist_manager.h"
#include "sds.h"
#include "streams.h"

// TODO rename this (and this file)
typedef struct MyData
{
	Player* player;
	Library* library;
	PlaylistManager* playlist_manager;
	StreamList* stream_list;
	sds library_json;
} MyData;
