#pragma once

#include <sys/types.h>
#include <unistd.h>

#define LOG_LEVEL_CRITICAL 1
#define LOG_LEVEL_ERROR 2
#define LOG_LEVEL_WARNING 3
#define LOG_LEVEL_INFO 4
#define LOG_LEVEL_DEBUG 5
#define LOG_LEVEL_TRACE 6

#define LOG_CRITICAL( fmt, args... )                                                               \
	LOG( LOG_LEVEL_CRITICAL, "CRITICAL", __FILE__, __LINE__, fmt, ##args )
#define LOG_ERROR( fmt, args... ) LOG( LOG_LEVEL_ERROR, "ERROR", __FILE__, __LINE__, fmt, ##args )
#define LOG_WARN( fmt, args... ) LOG( LOG_LEVEL_WARNING, "WARN", __FILE__, __LINE__, fmt, ##args )
#define LOG_INFO( fmt, args... ) LOG( LOG_LEVEL_INFO, "INFO", __FILE__, __LINE__, fmt, ##args )
#define LOG_DEBUG( fmt, args... ) LOG( LOG_LEVEL_DEBUG, "DEBUG", __FILE__, __LINE__, fmt, ##args )

#ifdef DEBUG_BUILD
#	define LOG_TRACE( fmt, args... )                                                              \
		LOG( LOG_LEVEL_TRACE, "TRACE", __FILE__, __LINE__, fmt, ##args )
#else
#	define LOG_TRACE( fmt, args... )                                                              \
		{}
#endif // DEBUG_BUILD

#define LOG( level_num, level_str, file, line, fmt, args... )                                      \
	do {                                                                                           \
		if( level_num <= menoetius_log_level ) {                                                   \
			_log( level_num,                                                                       \
				  "level=s pid=d file=s line=d " fmt,                                              \
				  level_str,                                                                       \
				  getpid(),                                                                        \
				  file,                                                                            \
				  line,                                                                            \
				  ##args );                                                                        \
		}                                                                                          \
	} while( 0 )

#define LOG_OK 0
#define LOG_ERROR_UNEXPECTED_CHAR 1
#define LOG_ERROR_OUT_OF_SPACE 2

extern int menoetius_log_level;

void _log( int level_num, const char* fmt, ... );

void set_log_level( int level );

void set_log_level_from_env_variables( const char** env );

void set_log_level_from_string( const char* s );

struct log_context;
typedef struct log_context log_context;

log_context* new_log_context( log_context* ctx, const char* fmt, ... );
void free_log_context( log_context* ctx );

//_log("level=%s src.file=%s src.line=%s " fmt, level, __LINE__, __FILE__, ##args);
