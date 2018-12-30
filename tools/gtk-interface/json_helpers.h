#ifndef _JSON_HELPERS_H_
#define _JSON_HELPERS_H_

#include <json-c/json.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <stdbool.h>

size_t write_data(char *ptr, size_t size, size_t nmemb, void *user_data);

json_bool get_json_object( struct json_object *obj, const char *key, enum json_type expected_type, struct json_object **value );

json_bool get_json_object_string( struct json_object *obj, const char *key, GString **s );

json_bool get_json_object_int( struct json_object *obj, const char *key, int *val );

json_bool get_json_object_double( struct json_object *obj, const char *key, double *val );

json_bool get_json_object_bool( struct json_object *obj, const char *key, bool *val );

GString* get_json_object_string_default( struct json_object *obj, const char *key, const char *default_val );

int get_json_object_int_default( struct json_object *obj, const char *key, int default_val );

double get_json_object_double_default( struct json_object *obj, const char *key, double default_val );

#endif // _JSON_HELPERS_H_
