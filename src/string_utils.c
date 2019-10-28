#define _XOPEN_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "log.h"
#include "string_utils.h"

#include <ctype.h>
#include <string.h>

bool has_suffix( const char* s, const char* suffix )
{
	size_t s_len = strlen( s );
	size_t suffix_len = strlen( suffix );

	if( suffix_len > s_len )
		return 0;

	return 0 == strncmp( s + s_len - suffix_len, suffix, suffix_len );
}

bool has_prefix( const char* s, const char* prefix )
{
	return strncmp( prefix, s, strlen( prefix ) ) == 0;
}

const char* trim_prefix( const char* s, const char* prefix )
{
	if( has_prefix( s, prefix ) ) {
		return s + strlen( prefix );
	}
	return s;
}

bool trim_suffix( char* s, const char* suffix )
{
	size_t s_len = strlen( s );
	size_t suffix_len = strlen( suffix );

	if( suffix_len > s_len )
		return false;

	if( 0 != strncmp( s + s_len - suffix_len, suffix, suffix_len ) ) {
		return false;
	}

	s[s_len - suffix_len] = '\0';
	return true;
}

const char* empty_str = "";
const char* null_to_empty( char* s )
{
	if( s == NULL )
		return empty_str;
	return s;
}

void str_to_upper( char* s )
{
	while( *s ) {
		*s = toupper( (unsigned char)*s );
		s++;
	}
}

int32_t parse_date_to_epoch_days( const char* s )
{
	struct tm tm = {0};

	if( strptime( s, "%Y-%m-%d", &tm ) != NULL ) {
		// pass
	}
	else if( strptime( s, "%Y-%m", &tm ) != NULL ) {
		// pass
	}
	else if( strptime( s, "%Y", &tm ) != NULL ) {
		// pass
	}
	else {
		printf( "failed to parse %s\n", s );
		assert( 0 );
	}
	time_t t = mktime( &tm );
	time_t days = t / ( 60 * 60 * 24 );

	//LOG_INFO("t=d year=d month=d day=d hour=d min=d sec=d tm", t, tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	//LOG_INFO("date=s days=d", s, days);

	return (int32_t)days;
}
