#include "log.h"

int _log(const char *fmt, ...)
{
	char buf[1024];
	va_list args;
	va_start( args, fmt );
	vsnprintf( buf, 1024, fmt, args );
	va_end( args );

	fputs( buf, stderr );
	fflush( stderr );
}
