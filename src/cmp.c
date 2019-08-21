#include "cmp.h"

#include <ctype.h>

int stricmp( const char* a, const char* b )
{
	for( ;; a++, b++ ) {
		char aa = tolower( *a );
		char bb = tolower( *b );
		if( aa == bb ) {
			if( aa == '\0' ) {
				return 0;
			}
			continue;
		}
		else if( aa < bb ) {
			return -1;
		}
		else {
			return 1;
		}
	}
}
