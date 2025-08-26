#include "streamer.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define RUN(test)                                                                                                      \
	do {                                                                                                               \
		printf("%-35s", #test);                                                                                        \
		test();                                                                                                        \
		puts("OK");                                                                                                    \
	} while (0)

#define ASSERT(expr) assert(expr)

static char tmpdir_template[] = "/tmp/streamer_test_XXXXXX";
static char *g_tmpdir;

static char *make_path(const char *name) {
	char *path = malloc(PATH_MAX);
	snprintf(path, PATH_MAX, "%s/%s", g_tmpdir, name);
	return path;
}

static void write_file(const char *name, const uint8_t *data, size_t len) {
	char *path = make_path(name);
	FILE *f = fopen(path, "wb");
	ASSERT(f);
	ASSERT(fwrite(data, 1, len, f) == len);
	fclose(f);
	free(path);
}

static void test_open_close(void) {
	struct streamer s;

	// fail on missing file
	ASSERT(!streamer_open(&s, "/no/such/file"));

	// succeed on a one-byte file
	static const uint8_t data1[] = {'Z'};
	write_file("one.txt", data1, 1);
	char *p1 = make_path("one.txt");
	ASSERT(streamer_open(&s, p1));
	streamer_close(&s);
	// after close, all fields zeroed
	ASSERT(s.handle == nullptr && s.len == 0 && s.pos == 0);
	free(p1);
}

static void test_peek_next_eof(void) {
	struct streamer s;
	static const char *lines = "ab\nc";
	write_file("small.txt", (const uint8_t *)lines, strlen(lines));
	char *ps = make_path("small.txt");
	ASSERT(streamer_open(&s, ps));

	// initial state
	ASSERT(!streamer_eof(&s));
	// peek doesn't advance
	uint8_t c = streamer_peek(&s);
	ASSERT(c == 'a');
	struct source_position pos0 = streamer_position(&s);
	ASSERT(pos0.offset == 0 && pos0.line == 1 && pos0.column == 1);

	// consume 'a'
	c = streamer_next(&s);
	ASSERT(c == 'a');
	struct source_position pos1 = streamer_position(&s);
	ASSERT(pos1.offset == 1 && pos1.line == 1 && pos1.column == 2);

	// consume 'b'
	c = streamer_next(&s);
	ASSERT(c == 'b');
	// consume newline
	c = streamer_next(&s);
	ASSERT(c == '\n');
	struct source_position pos2 = streamer_position(&s);
	ASSERT(pos2.offset == 3 && pos2.line == 2 && pos2.column == 1);

	// consume 'c'
	ASSERT(streamer_peek(&s) == 'c');
	c = streamer_next(&s);
	ASSERT(c == 'c');
	struct source_position pos3 = streamer_position(&s);
	ASSERT(pos3.offset == 4 && pos3.line == 2 && pos3.column == 2);

	// now at EOF
	ASSERT(streamer_eof(&s));
	ASSERT(streamer_peek(&s) == -1);
	ASSERT(streamer_next(&s) == -1);

	streamer_close(&s);
	free(ps);
}

static void test_seek_unget(void) {
	struct streamer s;
	static const char *txt = "abcdef";
	write_file("abc.txt", (const uint8_t *)txt, strlen(txt));
	char *pa = make_path("abc.txt");
	ASSERT(streamer_open(&s, pa));

	// absolute seek
	ASSERT(streamer_seek(&s, 3));
	ASSERT(streamer_position(&s).offset == 3);
	ASSERT(streamer_peek(&s) == 'd');
	ASSERT(streamer_next(&s) == 'd');

	// unget one
	ASSERT(streamer_unget(&s));
	ASSERT(streamer_position(&s).offset == 3);
	ASSERT(streamer_peek(&s) == 'd');

	// unget twice
	ASSERT(streamer_unget(&s)); // back to offset=3->2
	ASSERT(streamer_position(&s).offset == 2);
	ASSERT(streamer_peek(&s) == 'c');

	// cannot unget at start
	ASSERT(streamer_seek(&s, 0));
	ASSERT(!streamer_unget(&s));

	// seek beyond end fails
	ASSERT(!streamer_seek(&s, 100));

	streamer_close(&s);
	free(pa);
}

static void test_blob(void) {
	struct streamer s;
	static const char *nums = "0123456789";
	write_file("nums.txt", (const uint8_t *)nums, strlen(nums));
	char *pn = make_path("nums.txt");
	ASSERT(streamer_open(&s, pn));

	// middle of file
	ASSERT(streamer_seek(&s, 2));
	struct streamer_blob b1 = streamer_get_blob(&s);
	ASSERT(b1.cache[0] == '0');
	ASSERT(b1.cache[1] == '1');
	ASSERT(b1.cache[2] == '2');
	ASSERT(b1.cache[3] == '3');
	ASSERT(b1.cache[4] == '4');

	// start of file
	ASSERT(streamer_seek(&s, 0));
	struct streamer_blob b2 = streamer_get_blob(&s);
	ASSERT(b2.cache[2] == '0');
	ASSERT(b2.cache[3] == '1');
	ASSERT(b2.cache[4] == '2');

	// near end of file
	ASSERT(streamer_seek(&s, 9));
	struct streamer_blob b3 = streamer_get_blob(&s);
	ASSERT(b3.cache[0] == '7');
	ASSERT(b3.cache[1] == '8');
	ASSERT(b3.cache[2] == '9');

	streamer_close(&s);
	free(pn);
}

static void test_buffer_refill(void) {
	struct streamer s;
	size_t big = STREAMER_BUFFER_SIZE * 2 + 5;
	uint8_t *data = malloc(big);
	for (size_t i = 0; i < big; i++)
		data[i] = (uint8_t)(i & 0xFF);
	write_file("big.bin", data, big);
	free(data);

	char *pb = make_path("big.bin");
	ASSERT(streamer_open(&s, pb));

	// seek into second buffer chunk
	size_t off = STREAMER_BUFFER_SIZE + 3;
	ASSERT(streamer_seek(&s, off));
	ASSERT(streamer_position(&s).offset == off);
	uint8_t c = streamer_peek(&s);
	ASSERT(c == (uint8_t)(off & 0xFF));
	ASSERT(streamer_next(&s) == c);
	ASSERT(s.pos == off + 1);

	// unget across boundary
	ASSERT(streamer_unget(&s));
	ASSERT(streamer_position(&s).offset == off);
	ASSERT(streamer_peek(&s) == (uint8_t)(off & 0xFF));

	streamer_close(&s);
	free(pb);
}

int main(void) {
	setvbuf(stdout, nullptr, _IONBF, 0);

	g_tmpdir = mkdtemp(tmpdir_template);
	ASSERT(g_tmpdir);

	puts("\n=== STREAMER Functional Tests ===");

	RUN(test_open_close);
	RUN(test_peek_next_eof);
	RUN(test_seek_unget);
	RUN(test_blob);
	RUN(test_buffer_refill);

	puts("\nAll tests passed successfully!");
	rmdir(g_tmpdir);
	return 0;
}
