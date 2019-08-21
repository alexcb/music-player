#pragma once

#include <stdlib.h>
#include <string.h>

void* my_malloc( size_t n );
void* my_realloc( void* p, size_t n );
char* my_strdup( const char* s );
void my_free( void* p );

void my_malloc_init();
void my_malloc_free();
void my_malloc_assert_free();
