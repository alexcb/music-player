#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "sds.h"
#include "sglib.h"

typedef struct ID3Cache ID3Cache;
typedef struct StreamList StreamList;

struct Artist;
struct Album;
struct Track;

typedef struct Track
{
	sds path;
	sds title;
	struct Album* album;
	//uint32_t track;
	float length;

	// used to order tracks in an album
	struct Track* next_ptr;

	// used to quickly lookup a track based on the path
	char color_field;
	struct Track* left;
	struct Track* right;
} Track;
//#define TRACK_COMPARATOR(e1, e2) (e1->track - e2->track)
#define TRACK_PATH_COMPARATOR( e1, e2 ) ( strcmp( e1->path, e2->path ) )

SGLIB_DEFINE_RBTREE_PROTOTYPES( Track, left, right, color_field, TRACK_PATH_COMPARATOR )

typedef struct Album
{
	struct Artist* artist;
	sds album;
	sds path;
	int32_t release_date; // days since 1970

	Track* tracks;

	char color_field;
	struct Album* left;
	struct Album* right;
} Album;

#define ALBUM_CMPARATOR( x, y ) strcmp( ( x )->path, ( y )->path )
SGLIB_DEFINE_RBTREE_PROTOTYPES( Album, left, right, color_field, ALBUM_CMPARATOR )

typedef struct Artist
{
	sds artist;
	sds path;

	Album* albums;

	char color_field;
	struct Artist* left;
	struct Artist* right;
} Artist;

#define ARTIST_PATH_CMPARATOR( x, y ) strcmp( ( x )->path, ( y )->path )
SGLIB_DEFINE_RBTREE_PROTOTYPES( Artist, left, right, color_field, ARTIST_PATH_CMPARATOR )

typedef struct Library
{
	Artist* artists;
	Track* root_track;
	ID3Cache* id3_cache;
	sds library_path;
} Library;

int library_init( Library* library, ID3Cache* cache, const char* library_path );
int library_load( Library* library );
int library_get_track( Library* library, const char* path, Track** track );
