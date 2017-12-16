#include "string_utils.h"

#include <string.h>

bool has_suffix(const char *s, const char *suffix) {
	size_t s_len = strlen(s);
	size_t suffix_len = strlen(suffix);

	if(suffix_len > s_len)
		return 0;

	return 0 == strncmp( s + s_len - suffix_len, suffix, suffix_len );
}

bool has_prefix(const char *s, const char *prefix) {
	return strncmp(prefix, s, strlen(prefix)) == 0;
}

const char* trim_prefix(const char *s, const char *prefix) {
	if( has_prefix(s, prefix) ) {
		return s + strlen(prefix);
	}
	return s;
}

bool trim_suffix(char *s, const char *suffix) {
	size_t s_len = strlen(s);
	size_t suffix_len = strlen(suffix);

	if(suffix_len > s_len)
		return false;

	if( 0 != strncmp( s + s_len - suffix_len, suffix, suffix_len ) ) {
		return false;
	}

	s[ s_len - suffix_len ] = '\0';
	return true;
}

const char* empty_str = "";
const char* null_to_empty(char *s) {
	if( s == NULL )
		return empty_str;
	return s;
}
