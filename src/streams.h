#pragma once

#include <stddef.h>
#include <stdbool.h>

#include "sglib.h"
#include "sds.h"

typedef struct ID3Cache ID3Cache;

typedef struct Stream
{
	sds name;
	sds url;

    struct Stream *next_ptr;
} Stream;

typedef struct StreamList
{
	Stream *p;
} StreamList;

#define STREAM_COMPARATOR(e1, e2) (strcmp(e1->name, e2->name))

int parse_streams(const char *path, StreamList *sl);
