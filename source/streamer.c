#include <streamer.h>
#include <string.h>

static bool refill_buffer(struct streamer *s) {
	if (!s->handle)
		return false;
	if (fseek(s->handle, (long)s->buffer_start, SEEK_SET) != 0)
		return false;

	s->buffer_len = fread(s->buffer, 1, STREAMER_BUFFER_SIZE, s->handle);
	s->buffer_pos = s->pos - s->buffer_start;
	if (s->buffer_pos > s->buffer_len)
		s->buffer_pos = s->buffer_len;
	return true;
}

bool streamer_open(struct streamer *s, const char *filename) {
	if (!s)
		return false;
	memset(s, 0, sizeof *s);

	s->filename = filename;
	s->line = s->column = 1;

	s->handle = fopen(filename, "rb");
	if (!s->handle)
		return false;

	if (fseek(s->handle, 0, SEEK_END) != 0)
		goto fail;
	long fsz = ftell(s->handle);
	if (fsz < 0)
		goto fail;
	s->len = (size_t)fsz;

	if (fseek(s->handle, 0, SEEK_SET) != 0)
		goto fail;
	s->buffer_start = 0;
	if (!refill_buffer(s))
		goto fail;

	return true;

fail:
	fclose(s->handle);
	memset(s, 0, sizeof *s);
	return false;
}

void streamer_close(struct streamer *s) {
	if (!s)
		return;
	if (s->handle)
		fclose(s->handle);
	memset(s, 0, sizeof *s);
}

bool streamer_eof(const struct streamer *s) { return s->pos >= s->len; }

bool streamer_seek(struct streamer *s, size_t offset) {
	if (offset > s->len)
		return false;

	s->pos = 0;
	s->buffer_start = 0;
	s->buffer_len = 0;
	s->buffer_pos = 0;
	s->line = 1;
	s->column = 1;
	s->pushback_len = 0;

	if (!refill_buffer(s))
		return false;

	while (s->pos < offset) {
		if (streamer_next(s) < 0)
			return false;
	}
	return true;
}

int streamer_peek(struct streamer *s) {
	if (s->pushback_len > 0)
		return s->pushback_buf[s->pushback_len - 1];

	if (s->pos >= s->len)
		return -1;

	if (s->buffer_pos >= s->buffer_len) {
		s->buffer_start = s->pos - (s->pos % STREAMER_BUFFER_SIZE);
		if (!refill_buffer(s) || s->buffer_len == 0)
			return -1;
	}

	return (int)s->buffer[s->buffer_pos];
}

int streamer_next(struct streamer *s) {
	if (s->pushback_len > 0) {
		uint8_t c = s->pushback_buf[--s->pushback_len];
		s->line = s->pushback_line[s->pushback_len];
		s->column = s->pushback_column[s->pushback_len];

		s->pos++;
		s->buffer_pos++;
		s->last_char = c;
		return (int)c;
	}

	int ci = streamer_peek(s);
	if (ci < 0)
		return -1;
	uint8_t c = (uint8_t)ci;

	s->prev_line = s->line;
	s->prev_column = s->column;

	s->pos++;
	s->buffer_pos++;
	s->last_char = c;

	if (c == '\n') {
		s->line++;
		s->column = 1;
	} else {
		s->column++;
	}

	return ci;
}

bool streamer_unget(struct streamer *s) {
	if (s->pos == 0 || s->pushback_len >= STREAMER_PUSHBACK_DEPTH)
		return false;

	s->pos--;
	if (s->buffer_pos > 0) {
		s->buffer_pos--;
	} else {
		s->buffer_start = s->pos - (s->pos % STREAMER_BUFFER_SIZE);
		if (!refill_buffer(s))
			return false;
		s->buffer_pos = s->pos - s->buffer_start;
	}

	size_t idx = s->pushback_len++;
	s->pushback_buf[idx] = s->last_char;
	s->pushback_line[idx] = s->prev_line;
	s->pushback_column[idx] = s->prev_column;

	return true;
}

struct source_position streamer_position(const struct streamer *s) {
	return (struct source_position){.filename = s->filename, .line = s->line, .column = s->column, .offset = s->pos};
}

struct streamer_blob streamer_get_blob(struct streamer *s) {
	struct streamer_blob b = {{0}};
	long start = (long)s->pos - 2;
	if (start < 0)
		start = 0;

	if ((size_t)start >= s->buffer_start && (size_t)(start + 5) <= s->buffer_start + s->buffer_len) {
		size_t off = (size_t)(start - s->buffer_start);
		memcpy(b.cache, &s->buffer[off], 5);
		return b;
	}

	long cur = ftell(s->handle);
	if (fseek(s->handle, start, SEEK_SET) == 0) {
		fread(b.cache, 1, 5, s->handle);
	}
	fseek(s->handle, cur, SEEK_SET);
	return b;
}
