#ifndef STREAMER_H
#define STREAMER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/**
 * streamer.h
 *
 * A simple buffered byte‐stream reader for files or in-memory buffers.
 */

#define STREAMER_BUFFER_SIZE 8192

struct source_position {
	const char *filename;
	size_t line, column;
	size_t offset;
};

struct source_span {
	struct source_position start, end;
};

struct streamer {
	const char *filename;
	FILE *handle;

	const uint8_t buffer[STREAMER_BUFFER_SIZE];
	size_t buffer_offset; /* << buffer offset inside buffer */
	size_t buffer_loc;	  /* << offset in file where buffer starts */

	size_t len;			 /* << full file length */
	size_t pos;			 /* << real in-file position in bytes */
	size_t line, column; /* read in-file line and column */
};

/**
 * A small window around the current byte:
 *  cache[0..4] holds: 2 bytes before, the current byte, and 2 bytes after.
 */
struct streamer_blob {
	uint8_t cache[5];
};

/* open a file-backed streamer */
bool streamer_open(struct streamer *s, const char *filename);
/* close a file-backed streamer and free internal state */
void streamer_close(struct streamer *s);

/* move the read position to absolute byte offset */
bool streamer_seek(struct streamer *s, size_t offset);
/* push the read position back by one byte */
bool streamer_unget(struct streamer *s);

/* peek at the next byte without advancing */
uint8_t streamer_peek(struct streamer *s);
/* read and consume the next byte */
uint8_t streamer_next(struct streamer *s);

/* retrieve a 5-byte context blob around the current pos */
struct streamer_blob streamer_get_blob(struct streamer *s);

/* get the current source_position (filename, line, column, offset) */
struct source_position streamer_position(struct streamer *s);
/* test for end-of-file */
bool streamer_eof(struct streamer *s);

#endif /* STREAMER_H */
