#ifndef STREAMER_H
#define STREAMER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define STREAMER_BUFFER_SIZE 8192
#define STREAMER_PUSHBACK_DEPTH 8

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

	/* main file buffer */
	uint8_t buffer[STREAMER_BUFFER_SIZE];
	size_t buffer_start; /* file‐offset of buffer[0] */
	size_t buffer_len;	 /* bytes actually in buffer */
	size_t buffer_pos;	 /* next index in buffer[] to read */

	size_t len;			 /* total file length */
	size_t pos;			 /* absolute file offset = buffer_start + buffer_pos */
	size_t line, column; /* current line/column */

	/* multi‐char pushback stack */
	uint8_t pushback_buf[STREAMER_PUSHBACK_DEPTH];
	size_t pushback_len;
	size_t pushback_line[STREAMER_PUSHBACK_DEPTH];
	size_t pushback_column[STREAMER_PUSHBACK_DEPTH];

	/* last‐read character & its position (for unget) */
	uint8_t last_char;
	size_t prev_line, prev_column;
};

/* open a file‐backed streamer */
bool streamer_open(struct streamer *s, const char *filename);
/* close the streamer */
void streamer_close(struct streamer *s);

/* absolute seek (byte offset), recomputes line/col, clears pushback */
bool streamer_seek(struct streamer *s, size_t offset);
/* push back the last character read (up to STREAMER_PUSHBACK_DEPTH deep) */
bool streamer_unget(struct streamer *s);

/* peek at next byte without consuming; returns 0..255, or -1 on EOF/error */
int streamer_peek(struct streamer *s);
/* read & consume next byte (advances line/col); returns 0..255, or -1 on EOF/error */
int streamer_next(struct streamer *s);

/* grab a 5‐byte context window around the current pos */
struct streamer_blob {
	uint8_t cache[5];
};
struct streamer_blob streamer_get_blob(struct streamer *s);

/* report filename/line/col/offset */
struct source_position streamer_position(const struct streamer *s);
/* true if we've passed EOF */
bool streamer_eof(const struct streamer *s);

#endif /* STREAMER_H */
