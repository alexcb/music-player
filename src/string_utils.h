#include <inttypes.h>
#include <stdbool.h>

bool has_suffix( const char* s, const char* suffix );

bool has_prefix( const char* s, const char* prefix );

const char* trim_prefix( const char* s, const char* prefix );

bool trim_suffix( char* s, const char* suffix );

const char* null_to_empty( char* s );

void str_to_upper( char* s );

int32_t parse_date_to_epoch_days( const char* s );
