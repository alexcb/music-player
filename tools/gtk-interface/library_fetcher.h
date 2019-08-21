#ifndef _LIBRARY_FETCHER_H_
#define _LIBRARY_FETCHER_H_

#include <glib.h>

struct track;
struct album;
struct artist;

typedef struct track
{
	int number;
	int duration;
	GString* title;
	GString* path;
	struct album* album;
} Track;

typedef struct album
{
	int year;
	GString* title;
	Track* tracks;
	int num_tracks;
	struct artist* artist;
} Album;

typedef struct artist
{
	GString* artist;
	Album* albums;
	int num_albums;
} Artist;

typedef struct library
{
	Artist* artists;
	int num_artists;
	GHashTable* path_lookup;
} Library;

int fetch_library( const char* endpoint, Library** library );

int library_lookup_by_path( Library* library, const char* path, Track** track );

#endif // _LIBRARY_FETCHER_H_
