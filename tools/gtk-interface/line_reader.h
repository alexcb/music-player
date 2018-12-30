#ifndef _LINE_READER_H_
#define _LINE_READER_H_

#include <stddef.h>

#define BUFFERED_STREAM_OK 0
#define BUFFERED_STREAM_EAGAIN 1
#define BUFFERED_STREAM_EUNKNOWN 2

typedef struct {
	int fd;
	char *buf;
	size_t cap;
	int i;
} BufferedStreamer;

int init_buffered_stream( BufferedStreamer *self, int fd, size_t cap );

int reset_buffered_stream( BufferedStreamer *self, int fd );

int read_line( BufferedStreamer *self, char *buf, size_t *n );

int read_bytes( BufferedStreamer *self, size_t n, char *buf );

#endif // _LINE_READER_H_
