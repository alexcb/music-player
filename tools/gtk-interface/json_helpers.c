#include "json_helpers.h"

size_t write_data( char* ptr, size_t size, size_t nmemb, void* user_data )
{
	GString* s = (GString*)user_data;
	size_t n = size * nmemb;
	g_string_append_len( s, ptr, n );
	//fprintf(stderr, "data at %p size=%ld nmemb=%ld\n", ptr, size, nmemb);
	return n;
}

int get_json_object( struct json_object* obj,
					 const char* key,
					 enum json_type expected_type,
					 struct json_object** value )
{
	if( !json_object_object_get_ex( obj, key, value ) ) {
		return 0;
	}
	if( !json_object_is_type( *value, expected_type ) ) {
		return 0;
	}
	return 1;
}

int get_json_object_string( struct json_object* obj, const char* key, GString** s )
{
	struct json_object* value;
	if( !get_json_object( obj, key, json_type_string, &value ) ) {
		return 0;
	}
	*s = g_string_new( json_object_get_string( value ) );
	return 1;
}

int get_json_object_int( struct json_object* obj, const char* key, int* val )
{
	struct json_object* value;
	if( !get_json_object( obj, key, json_type_int, &value ) ) {
		return 0;
	}
	*val = json_object_get_int( value );
	return 1;
}

int get_json_object_double( struct json_object* obj, const char* key, double* val )
{
	struct json_object* value;
	if( !get_json_object( obj, key, json_type_double, &value ) ) {
		return 0;
	}
	*val = json_object_get_double( value );
	return 1;
}

int get_json_object_bool( struct json_object* obj, const char* key, bool* val )
{
	struct json_object* value;
	if( !get_json_object( obj, key, json_type_boolean, &value ) ) {
		return 0;
	}
	*val = json_object_get_boolean( value );
	return 1;
}

GString*
get_json_object_string_default( struct json_object* obj, const char* key, const char* default_val )
{
	struct json_object* value;
	if( !get_json_object( obj, key, json_type_string, &value ) ) {
		return g_string_new( default_val );
	}
	return g_string_new( json_object_get_string( value ) );
}

int get_json_object_int_default( struct json_object* obj, const char* key, int default_val )
{
	struct json_object* value;
	if( !get_json_object( obj, key, json_type_int, &value ) ) {
		return default_val;
	}
	return json_object_get_int( value );
}

double
get_json_object_double_default( struct json_object* obj, const char* key, double default_val )
{
	struct json_object* value;
	if( !get_json_object( obj, key, json_type_double, &value ) ) {
		return default_val;
	}
	return json_object_get_double( value );
}
