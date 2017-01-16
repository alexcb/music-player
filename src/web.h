#pragma once

#include "my_data.h"
#include <stdbool.h>

void update_metadata_web_clients(bool playing, const char *playlist_name, const PlayerTrackInfo *track, void *data);

int start_http_server( AlbumList *album_list, PlaylistManager *playlist_manager, Player *player );
