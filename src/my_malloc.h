#pragma once

#include <stdlib.h>
#include <string.h>

#ifdef USE_RASP_PI

#	define my_malloc malloc
#	define my_realloc realloc
#	define my_strdup strdup
#	define my_free free

#	define my_malloc_init() ;
#	define my_malloc_free() ;
#	define my_malloc_assert_free() ;

#else

void* my_malloc( size_t n );
void* my_realloc( void* p, size_t n );
char* my_strdup( const char* s );
void my_free( void* p );

void my_malloc_init();
void my_malloc_free();
void my_malloc_assert_free();

#endif // USE_RASP_PI
