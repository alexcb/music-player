#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "sglib.h"
#include "sds.h"

typedef struct ID3Cache ID3Cache;
typedef struct StreamList StreamList;

typedef struct Track
{
	sds artist;
	sds title;
	sds path;
	sds album;
	uint32_t track;
	float length;

	// used to order tracks in an album
	struct Track *next_ptr;

	// used to quickly lookup a track based on the path
	char color_field;
	struct Track *left;
	struct Track *right;
} Track;
//#define TRACK_COMPARATOR(e1, e2) (e1->track - e2->track)
#define TRACK_PATH_COMPARATOR(e1, e2) (strcmp(e1->path, e2->path))

SGLIB_DEFINE_RBTREE_PROTOTYPES(Track, left, right, color_field, TRACK_PATH_COMPARATOR)


typedef struct Album
{
	sds artist;
	sds album;
	sds path;
	uint32_t year;

	Track *tracks;

	char color_field;
	struct Album *left;
	struct Album *right;
} Album;

#define ALBUM_CMPARATOR(x,y) strcmp((x)->path, (y)->path)
SGLIB_DEFINE_RBTREE_PROTOTYPES(Album, left, right, color_field, ALBUM_CMPARATOR)


typedef struct Artist
{
	sds artist;
	sds path;

	Album *albums;

	char color_field;
	struct Artist *left;
	struct Artist *right;
} Artist;

#define ARTIST_PATH_CMPARATOR(x,y) strcmp((x)->path, (y)->path)
SGLIB_DEFINE_RBTREE_PROTOTYPES(Artist, left, right, color_field, ARTIST_PATH_CMPARATOR)

typedef struct AlbumList
{
	Artist *root;
	Track *root_track;
	ID3Cache *id3_cache;
	sds library_path;
} AlbumList;

int album_list_init( AlbumList *album_list, ID3Cache *cache, const char *library_path );
int album_list_load( AlbumList *album_list );
int album_list_get_track( AlbumList *album_list, const char *path, Track **track );

