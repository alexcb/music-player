#pragma once

#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

#include "playlist.h"

#define PLAY_QUEUE_CAP 32

typedef struct PlayQueueItem
{
	char *buf_start;
	PlaylistItem *playlist_item;
} PlayQueueItem;


typedef struct PlayQueue
{
	int front, rear;
	PlayQueueItem queue[PLAY_QUEUE_CAP];
} PlayQueue;

int play_queue_init( PlayQueue *pq );
int play_queue_add( PlayQueue *pq, PlayQueueItem **item );
int play_queue_head( PlayQueue *pq, PlayQueueItem **item );
int play_queue_pop( PlayQueue *pq );
void play_queue_clear( PlayQueue *pq );
