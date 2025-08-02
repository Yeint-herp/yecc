#include "diag.h"
#include "streamer.h"
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define RUN(test)                                                                                                      \
	do {                                                                                                               \
		printf("\n=== %s ===\n", #test);                                                                               \
		test();                                                                                                        \
	} while (0)

static char tmpdir_template[] = "/tmp/diag_test_XXXXXX";
static char *test_dir = nullptr;

static char *make_file_path(const char *name) {
	char *path = malloc(PATH_MAX);
	snprintf(path, PATH_MAX, "%s/%s", test_dir, name);
	return path;
}

static void write_file(const char *name, const uint8_t *data, size_t len) {
	char *path = make_file_path(name);
	FILE *f = fopen(path, "wb");
	assert(f && "fopen failed");
	assert(fwrite(data, 1, len, f) == len && "fwrite failed");
	fclose(f);
	free(path);
}

static void test_error_single_line(void) {
	const char *code = "int main() {\n"
					   "    return 0\n"
					   "}\n";
	write_file("single.c", (const uint8_t *)code, strlen(code));

	char *path = make_file_path("single.c");
	struct source_span span = {.start = {.filename = path, .line = 2, .column = 12},
							   .end = {.filename = path, .line = 2, .column = 13}};
	diag_error(span, "expected ';' after return");
	free(path);
}

static void test_error_multiline(void) {
	const char *code = "void greet(void) {\n"
					   "    printf(\"Hello);   \n"
					   "}\n";
	write_file("multi.c", (const uint8_t *)code, strlen(code));

	char *path = make_file_path("multi.c");
	struct source_span span = {.start = {.filename = path, .line = 2, .column = 12},
							   .end = {.filename = path, .line = 3, .column = 1}};
	diag_error(span, "unterminated string literal");
	free(path);
}

static void test_warning_basic(void) {
	const char *code = "int main(void) {\n"
					   "    int x = 42;  // x unused\n"
					   "    return 0;\n"
					   "}\n";
	write_file("warn.c", (const uint8_t *)code, strlen(code));

	char *path = make_file_path("warn.c");
	struct source_span span = {.start = {.filename = path, .line = 2, .column = 9},
							   .end = {.filename = path, .line = 2, .column = 14}};
	diag_warning(span, "unused variable 'x'");
	free(path);
}

static void test_info_basic(void) {
	const char *code = "// A comment\n"
					   "int val = 100;\n";
	write_file("info.c", (const uint8_t *)code, strlen(code));

	char *path = make_file_path("info.c");
	struct source_span span = {.start = {.filename = path, .line = 2, .column = 5},
							   .end = {.filename = path, .line = 2, .column = 8}};
	diag_info(span, "declared here");
	free(path);
}

static void test_note_only(void) {
	const char *code = "int x;\n";
	write_file("note.c", (const uint8_t *)code, strlen(code));

	char *path = make_file_path("note.c");
	struct source_span span = {.start = {.filename = path, .line = 1, .column = 5},
							   .end = {.filename = path, .line = 1, .column = 6}};
	diag_note(span, "variable declared here");
	free(path);
}

static void test_error_with_note(void) {
	const char *code = "const int a = 10;\n"
					   "a = 20;  // write to const\n";
	write_file("const1.c", (const uint8_t *)code, strlen(code));

	char *path = make_file_path("const1.c");
	struct source_span err = {.start = {.filename = path, .line = 2, .column = 1},
							  .end = {.filename = path, .line = 2, .column = 2}};
	diag_error(err, "cannot assign to const variable");

	struct source_span note = {.start = {.filename = path, .line = 1, .column = 11},
							   .end = {.filename = path, .line = 1, .column = 12}};
	diag_context(DIAG_LEVEL_NOTE, note, "declared const here");
	free(path);
}

static void test_error_with_two_notes(void) {
	const char *code = "int foo = 5;\n"
					   "int bar = foo + baz;\n"
					   "       // baz undefined\n";
	write_file("undef.c", (const uint8_t *)code, strlen(code));

	char *path = make_file_path("undef.c");
	struct source_span err = {.start = {.filename = path, .line = 2, .column = 16},
							  .end = {.filename = path, .line = 2, .column = 19}};
	diag_error(err, "use of undeclared identifier 'baz'");

	struct source_span note1 = {.start = {.filename = path, .line = 1, .column = 5},
								.end = {.filename = path, .line = 1, .column = 8}};
	diag_context(DIAG_LEVEL_NOTE, note1, "did you mean 'foo'?");

	free(path);
}

static void test_error_multiline_span(void) {
	const char *code = "int sum = a +\n"
					   "          b +\n"
					   "          ;\n";
	write_file("multi_span.c", (const uint8_t *)code, strlen(code));

	char *path = make_file_path("multi_span.c");
	struct source_span span = {.start = {.filename = path, .line = 1, .column = 11},
							   .end = {.filename = path, .line = 3, .column = 11}};
	diag_error(span, "expression ends with semicolon");
	free(path);
}

static void test_arrow_at_start(void) {
	const char *code = "oops_error;\n";
	write_file("start.c", (const uint8_t *)code, strlen(code));

	char *path = make_file_path("start.c");
	struct source_span span = {.start = {.filename = path, .line = 1, .column = 1},
							   .end = {.filename = path, .line = 1, .column = 10}};
	diag_error(span, "unexpected token 'oops_error'");
	free(path);
}

static void test_zero_length_span(void) {
	const char *code = "int x;\n";
	write_file("zero.c", (const uint8_t *)code, strlen(code));

	char *path = make_file_path("zero.c");
	struct source_span span = {.start = {.filename = path, .line = 1, .column = 5},
							   .end = {.filename = path, .line = 1, .column = 5}};
	diag_error(span, "zero-length span demonstration");
	free(path);
}

static void test_context_only(void) {
	const char *code = "int lonely;\n";
	write_file("context_only.c", (const uint8_t *)code, strlen(code));

	char *path = make_file_path("context_only.c");
	struct source_span span = {.start = {.filename = path, .line = 1, .column = 5},
							   .end = {.filename = path, .line = 1, .column = 11}};
	diag_context(DIAG_LEVEL_NOTE, span, "just a standalone note");
	free(path);
}

static void test_long_message(void) {
	const char *code = "int main() {}\n";
	write_file("longmsg.c", (const uint8_t *)code, strlen(code));

	char *path = make_file_path("longmsg.c");
	struct source_span span = {.start = {.filename = path, .line = 1, .column = 5},
							   .end = {.filename = path, .line = 1, .column = 9}};
	diag_warning(span, "this is a very long warning message intended to test that long "
					   "diagnostic texts are handled gracefully and wrapped or truncated "
					   "appropriately by the diagnostics system");
	free(path);
}

int main(void) {
	diag_init();

	test_dir = mkdtemp(tmpdir_template);
	assert(test_dir && "mkdtemp failed");

	RUN(test_error_single_line);
	RUN(test_error_multiline);
	RUN(test_warning_basic);
	RUN(test_info_basic);
	RUN(test_note_only);
	RUN(test_error_with_note);
	RUN(test_error_with_two_notes);
	RUN(test_error_multiline_span);
	RUN(test_arrow_at_start);
	RUN(test_zero_length_span);
	RUN(test_context_only);
	RUN(test_long_message);

	char cmd[PATH_MAX + 16];
	snprintf(cmd, sizeof(cmd), "rm -rf %s", test_dir);
	system(cmd);
	return 0;
}
