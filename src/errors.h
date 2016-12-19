#pragma once

#define MAX_ERROR_STR_SIZE 1024

typedef enum {
	OK = 0,
	OUT_OF_MEMORY,
	PLAYLIST_MUTEX,
	FILESYSTEM_ERROR,
} errors;
