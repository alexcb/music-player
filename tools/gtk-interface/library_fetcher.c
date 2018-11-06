#include "library_fetcher.h"
#include "json_helpers.h"

#include <curl/curl.h>
#include <json-c/json.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>





int compare_artist(const void *s1, const void *s2)
{
  Artist *a1 = (Artist *)s1;
  Artist *a2 = (Artist *)s2;
  return g_strcmp0( a1->artist->str, a2->artist->str );
}

int parse_library( const char *s, Library **library )
{
	struct json_object *root_obj, *album_obj, *artist_obj, *tracks_obj, *track_obj;
	int num_albums, num_tracks;

	Artist *artist;

	GString *path, *title;

	*library = malloc(sizeof(Library));
	(**library).path_lookup = g_hash_table_new( g_str_hash, g_str_equal );

	root_obj = json_tokener_parse( s );

	json_object *albums, *json_artists;
	assert( get_json_object(root_obj, "artists", json_type_array, &json_artists) );

	int num_artists = json_object_array_length( json_artists );
	(**library).artists = malloc(sizeof(Artist)*num_artists);
	(**library).num_artists = num_artists;
	for( int i = 0; i < num_artists; i++ ) {
		artist = &((**library).artists[i]);
		artist_obj = json_object_array_get_idx( json_artists, i );

		artist->artist = get_json_object_string_default( artist_obj, "artist", "unknown artist" );
		printf("adding artist %d %s\n", i, artist->artist->str);

		assert( json_object_object_get_ex(artist_obj, "albums", &albums) );

		num_albums = json_object_array_length( albums );
		artist->albums = (Album*) malloc(sizeof(Album)*num_albums);
		artist->num_albums = num_albums;
		for( int j = 0; j < num_albums; j++ ) {
			album_obj = json_object_array_get_idx( albums, j );

			artist->albums[j].title = get_json_object_string_default( album_obj, "album", "unknown album" );
			artist->albums[j].year = get_json_object_int_default( album_obj, "year", 0 );
			printf("adding album %d.%d %s\n", i, j, artist->albums[j].title->str );

			assert( get_json_object( album_obj, "tracks", json_type_array, &tracks_obj ) );

			num_tracks = json_object_array_length( tracks_obj );
			artist->albums[j].num_tracks = num_tracks;

			Track *tracks = malloc(sizeof(Track)*num_tracks);
			for( int k = 0; k < num_tracks; k++ ) {
				track_obj = json_object_array_get_idx( tracks_obj, k );

				assert( get_json_object_string( track_obj, "path", &path ) );

				tracks[k].number = get_json_object_int_default( track_obj, "track_number", 0 );
				tracks[k].duration = (int) get_json_object_double_default( track_obj, "length", 0.f );
				tracks[k].album = &(artist->albums[j]);

				assert( get_json_object_string( track_obj, "title", &title ) );
				tracks[k].title = title;
				tracks[k].path = path;

				printf("adding track %d.%d.%d artist=%s album=%s track=%s path=%s\n", i, j, k, artist->artist->str, tracks[k].album->title->str, tracks[k].title->str, path->str );

				g_hash_table_insert( (**library).path_lookup, path->str, &tracks[k] );
			}
			artist->albums[j].tracks = tracks;
			artist->albums[j].artist = artist;
		}
	}
	
	// CAREFUL! if I sort this here, the pointers get messed up and tracks will say they are by the wrong artist
	//qsort((**library).artists, (**library).num_artists, sizeof(Artist), compare_artist);
	return 0;
}

int fetch_library( const char *endpoint, Library **library )
{
	int err = 0;
	CURL *curl;
	CURLcode res;

	char url[1024];
	snprintf( url, 1024, "%s/library", endpoint );
	printf("http get %s\n", url);

	curl = curl_easy_init();
	if(curl) {
		GString *payload = g_string_sized_new(1024*1024);

		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, payload);

		res = curl_easy_perform(curl);
		if(res != CURLE_OK) {
			fprintf(stderr, "curl_easy_perform() failed: %s\n",
					curl_easy_strerror(res));
			err = 1;
		} else {
			parse_library( payload->str, library );
		}

		g_string_free( payload, TRUE );
		curl_easy_cleanup( curl );
	}
	return err;
}

int library_lookup_by_path( Library *library, const char *path, Track **track )
{
	printf("looking up %s\n", path);
	gpointer p = g_hash_table_lookup( library->path_lookup, path );
	if( p == NULL ) {
		return 1;
	}

	*track = (Track *) p;

	return 0;
}
