#include <streamer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool refill_buffer(struct streamer *s) {
	if (!s->handle)
		return false;

	int orig_fd = fileno(s->handle);
	if (orig_fd < 0)
		return false;
	int dup_fd = dup(orig_fd);
	if (dup_fd < 0)
		return false;

	FILE *f = fdopen(dup_fd, "rb");
	if (!f) {
		close(dup_fd);
		return false;
	}

	if (fseek(f, (long)s->buffer_loc, SEEK_SET) != 0) {
		fclose(f);
		return false;
	}

	size_t n = fread((uint8_t *)s->buffer, 1, STREAMER_BUFFER_SIZE, f);
	fclose(f);

	if (n == 0) {
		return false;
	}

	s->buffer_offset = 0;
	return true;
}

bool streamer_open(struct streamer *s, const char *filename) {
	memset(s, 0, sizeof(*s));
	s->filename = filename;
	s->line = 1;
	s->column = 1;
	s->pos = 0;
	s->buffer_loc = 0;

	FILE *f = fopen(filename, "rb");
	if (!f)
		return false;

	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return false;
	}
	long file_len = ftell(f);
	if (file_len < 0) {
		fclose(f);
		return false;
	}
	s->len = (size_t)file_len;

	if (fseek(f, 0, SEEK_SET) != 0) {
		fclose(f);
		return false;
	}

	s->handle = f;
	return refill_buffer(s);
}

void streamer_close(struct streamer *s) {
	if (s && s->handle) {
		fclose(s->handle);
		s->handle = nullptr;
	}
	memset(s, 0, sizeof(*s));
}

bool streamer_eof(struct streamer *s) { return s->pos >= s->len; }

bool streamer_seek(struct streamer *s, size_t offset) {
	if (offset > s->len)
		return false;

	s->pos = offset;
	s->buffer_loc = offset - (offset % STREAMER_BUFFER_SIZE);
	s->line = 1;
	s->column = 1;

	return refill_buffer(s);
}

bool streamer_unget(struct streamer *s) {
	if (s->pos == 0)
		return false;
	s->pos--;

	size_t new_buf_loc = s->pos - (s->pos % STREAMER_BUFFER_SIZE);
	if (new_buf_loc != s->buffer_loc) {
		s->buffer_loc = new_buf_loc;
		if (!refill_buffer(s))
			return false;
	}

	s->buffer_offset = s->pos - s->buffer_loc;
	uint8_t c = ((uint8_t *)s->buffer)[s->buffer_offset];

	if (c == '\n') {
		if (s->line > 1)
			s->line--;

		long scan = (long)s->pos - 1;
		size_t col = 1;
		while (scan >= 0) {
			if (fseek(s->handle, scan, SEEK_SET) != 0)
				break;
			int ch = fgetc(s->handle);
			if (ch == '\n')
				break;
			col++;
			scan--;
		}
		s->column = col;
	} else {
		if (s->column > 1)
			s->column--;
	}

	return true;
}

uint8_t streamer_peek(struct streamer *s) {
	if (streamer_eof(s))
		return 0;

	size_t in_buf = s->pos - s->buffer_loc;
	if (in_buf >= STREAMER_BUFFER_SIZE) {
		s->buffer_loc = s->pos - (s->pos % STREAMER_BUFFER_SIZE);
		if (!refill_buffer(s))
			return 0;
	}

	s->buffer_offset = s->pos - s->buffer_loc;
	return ((uint8_t *)s->buffer)[s->buffer_offset];
}

uint8_t streamer_next(struct streamer *s) {
	if (streamer_eof(s))
		return 0;

	uint8_t c = streamer_peek(s);
	s->pos++;

	if (c == '\n') {
		s->line++;
		s->column = 1;
	} else {
		s->column++;
	}

	s->buffer_offset++;
	if (s->buffer_offset >= STREAMER_BUFFER_SIZE) {
		s->buffer_loc += STREAMER_BUFFER_SIZE;
		refill_buffer(s);
	}

	return c;
}

struct source_position streamer_position(struct streamer *s) {
	return (struct source_position){.filename = s->filename, .line = s->line, .column = s->column, .offset = s->pos};
}

struct streamer_blob streamer_get_blob(struct streamer *s) {
	struct streamer_blob blob = {{0}};
	long start = (long)s->pos - 2;
	if (start < 0)
		start = 0;

	if (s->handle) {
		int orig_fd = fileno(s->handle);
		if (orig_fd >= 0) {
			int dup_fd = dup(orig_fd);
			if (dup_fd >= 0) {
				FILE *f = fdopen(dup_fd, "rb");
				if (f) {
					if (fseek(f, start, SEEK_SET) == 0) {
						fread(blob.cache, 1, sizeof(blob.cache), f);
					}
					fclose(f);
					return blob;
				}
				close(dup_fd);
			}
		}
	}

	size_t local_start = (start >= (long)s->buffer_loc) ? (size_t)(start - s->buffer_loc) : 0;
	for (int i = 0; i < 5; i++) {
		size_t idx = local_start + i;
		if (idx < STREAMER_BUFFER_SIZE) {
			blob.cache[i] = ((uint8_t *)s->buffer)[idx];
		}
	}
	return blob;
}
