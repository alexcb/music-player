#include "log.h"

#include "errors.h"

#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>


#define LOG_BUF_SIZE 1024

int tokenize_key(const char *s, const char **tok, int *n)
{
	const char *start = s;
	while( *start && (*start == ' ' || *start == '\t') ) {
		start++;
	}
	const char *p = start;
	while( *p ) {
		if( *p == ' ' ) {
			return ERROR_UNEXPECTED_CHAR;
		}
		if( *p == '=' ) {
			if( start == p ) {
				return ERROR_UNEXPECTED_CHAR;
			}
			*tok = start;
			*n = p - start;
			return OK;
		}
		p++;
	}
	return ERROR_UNEXPECTED_CHAR;
}

int tokenize_value_fmt(const char *s, const char **tok, int *n)
{
	//if( s != '%' ) {
	//	return ERROR_UNEXPECTED_CHAR;
	//}
	//s++;

	const char *p = s;
	while( *p != ' ' && *p ) {
		p++;
	}
	if( s == p ) {
		return ERROR_UNEXPECTED_CHAR;
	}
	*tok = s;
	*n = p - s;
	return OK;
}

int append_quoted_string( char *buf, int buf_size, const char *s )
{
	bool needs_quotes = false;
	for( const char *p = s; *p; p++ ) {
		if( *p == ' ' || *p == '"' ) {
			needs_quotes = true;
			break;
		}
	}
	if( needs_quotes == false ) {
		strncpy( buf, s, buf_size );
		return strlen( s );
	}

	char *start = buf;
	if( buf_size <= 0 )
		return buf - start;
	buf[0] = '"';
	buf++;
	buf_size--;
	while( *s ) {
		if( *s == '\\' || *s == '"' ) {
			if( buf_size <= 0 )
				return buf - start;
			buf[0] = '\\';
			buf++;
			buf_size--;
		}
		if( buf_size <= 0 )
			return buf - start;
		buf[0] = *s;
		buf++;
		buf_size--;
		s++;
	}
	if( buf_size <= 0 )
		return buf - start;
	buf[0] = '"';
	buf++;
	buf_size--;
	return buf - start;
}

void _log(const char *fmt, ...)
{
	va_list arguments;
	int res;
	int key_n, val_fmt_n;
	const char *key, *val_fmt;

	int buf_i = 0;
	int n;
	char buf[LOG_BUF_SIZE];

	va_start( arguments, fmt );

	const char *p = fmt;
	while( 1 ) {
		res = tokenize_key( p, &key, &key_n );
		if( res != OK ) {
			break;
		}
		p = key + key_n;

		assert( *p == '=' );
		p++;

		res = tokenize_value_fmt( p, &val_fmt, &val_fmt_n );
		assert( res == OK );
		assert( val_fmt_n == 1 );
		p = val_fmt + val_fmt_n;

		if( LOG_BUF_SIZE - buf_i > key_n + 1 ) {
			strncpy( buf+buf_i, key, key_n );
			buf_i += key_n;
			buf[buf_i] = '=';
			buf_i++;
		}

		if( val_fmt[0] == 's' ) {
			const char *val = va_arg( arguments, const char* );
			n = append_quoted_string( buf+buf_i, LOG_BUF_SIZE - buf_i, val );
			buf_i += n;
		//} else {
		//	char fmt2[3] = { '%', val_fmt[0], '\0' };
		//	snprintf( buf+buf_i, LOG_BUF_SIZE - buf_i, fmt2, val );
		}

		if( LOG_BUF_SIZE - buf_i > 1 ) {
			buf[buf_i] = ' ';
			buf_i++;
		}
	}
	while( *p && *p == ' ' ) {
		p++;
	}
	if( *p ) {
		n = append_quoted_string( buf+buf_i, LOG_BUF_SIZE - buf_i, "msg=" );
		buf_i += n;
		n = append_quoted_string( buf+buf_i, LOG_BUF_SIZE - buf_i, p );
		buf_i += n;
	}
	while( buf_i > 1 && buf[buf_i-1] == ' ' ) {
		buf_i--;
	}
	buf[buf_i] = '\n';
	buf[buf_i+1] = '\0';

	va_end( arguments );

	fputs( buf, stderr );
	fflush( stderr );
}
