#include <diag.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ANSI_BOLD "\x1b[1m"
#define ANSI_RED "\x1b[31m"
#define ANSI_YELLOW "\x1b[33m"
#define ANSI_BLUE "\x1b[34m"
#define ANSI_GREEN "\x1b[32m"
#define ANSI_RESET "\x1b[0m"

static bool diag_inited = false;
static bool diag_use_color = false;

static const char *lvl_str(diag_level l) {
	switch (l) {
	case DIAG_LEVEL_ERROR:
		return "error";
	case DIAG_LEVEL_WARNING:
		return "warning";
	case DIAG_LEVEL_NOTE:
		return "note";
	case DIAG_LEVEL_INFO:
		return "info";
	}
	return "unknown";
}
static const char *lvl_color(diag_level l) {
	switch (l) {
	case DIAG_LEVEL_ERROR:
		return ANSI_BOLD ANSI_RED;
	case DIAG_LEVEL_WARNING:
		return ANSI_BOLD ANSI_YELLOW;
	case DIAG_LEVEL_NOTE:
		return ANSI_BOLD ANSI_BLUE;
	case DIAG_LEVEL_INFO:
		return ANSI_BOLD ANSI_GREEN;
	}
	return ANSI_BOLD;
}

static char *read_line(const char *fn, size_t want) {
	FILE *f = fopen(fn, "r");
	if (!f)
		return nullptr;
	char *buf = nullptr;
	size_t cap = 0;
	ssize_t len;
	size_t ln = 1;
	while ((len = getline(&buf, &cap, f)) >= 0) {
		if (ln++ == want) {
			if (len && buf[len - 1] == '\n')
				buf[len - 1] = '\0';
			fclose(f);
			return buf;
		}
	}
	free(buf);
	fclose(f);
	return nullptr;
}

static void print_context_msg(struct source_span sp, const char *label, const char *message) {
	size_t start = sp.start.line;
	size_t end = sp.end.line < start ? start : sp.end.line;

	size_t width = 0;
	for (size_t n = start; n <= end; n++) {
		size_t d = n, w = 0;
		do {
			d /= 10;
			w++;
		} while (d);
		if (w > width)
			width = w;
	}

	for (size_t ln = start; ln <= end; ln++) {
		char *src = read_line(sp.start.filename, ln);
		if (!src)
			src = strdup("");

		fprintf(stderr, " %*zu | %s\n", (int)width, ln, src);

		size_t col0 = (ln == sp.start.line ? sp.start.column : 1);
		size_t col1 = (ln == sp.end.line ? sp.end.column : strlen(src) + 1);
		if (col1 <= col0)
			col1 = col0 + 1;

		fprintf(stderr, " %*s | ", (int)width, "");
		for (size_t i = 1; i < col0; i++)
			fputc(' ', stderr);
		fputc('^', stderr);
		for (size_t i = col0 + 1; i < col1; i++)
			fputc('-', stderr);
		fputc('>', stderr);

		if (ln == sp.start.line && label && message) {
			fputc(' ', stderr);
			fprintf(stderr, "%s %s", label, message);
		}
		fputc('\n', stderr);

		free(src);
	}
}

void diag_init(void) {
	if (diag_inited)
		return;
	diag_inited = true;
	diag_use_color = isatty(fileno(stderr)) && !getenv("NO_COLOR");
	if (getenv("CLICOLOR_FORCE"))
		diag_use_color = true;
}

static void diag_vreport(diag_level lvl, struct source_span sp, const char *fmt, va_list ap) {
	if (!diag_inited)
		diag_init();

	if (diag_use_color) {
		fputs(ANSI_BOLD "yecc:" ANSI_RESET " ", stderr);
	} else {
		fputs("yecc: ", stderr);
	}
	fprintf(stderr, "%s:%zu:%zu\n", sp.start.filename, sp.start.line, sp.start.column);

	char labelbuf[64];
	const char *col = diag_use_color ? lvl_color(lvl) : "";
	const char *rcol = diag_use_color ? ANSI_RESET : "";
	snprintf(labelbuf, sizeof(labelbuf), "%s%s:%s", col, lvl_str(lvl), rcol);

	char msgbuf[1024];
	vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);

	print_context_msg(sp, labelbuf, msgbuf);
}

void diag_error(struct source_span s, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	diag_vreport(DIAG_LEVEL_ERROR, s, fmt, ap);
	va_end(ap);
}
void diag_warning(struct source_span s, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	diag_vreport(DIAG_LEVEL_WARNING, s, fmt, ap);
	va_end(ap);
}
void diag_note(struct source_span s, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	diag_vreport(DIAG_LEVEL_NOTE, s, fmt, ap);
	va_end(ap);
}
void diag_info(struct source_span s, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	diag_vreport(DIAG_LEVEL_INFO, s, fmt, ap);
	va_end(ap);
}

void diag_context(diag_level lvl, struct source_span s, const char *fmt, ...) {
	if (!diag_inited)
		diag_init();

	char labelbuf[64];
	const char *col = diag_use_color ? lvl_color(lvl) : "";
	const char *rcol = diag_use_color ? ANSI_RESET : "";
	snprintf(labelbuf, sizeof(labelbuf), "%s%s:%s", col, lvl_str(lvl), rcol);

	char msgbuf[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(msgbuf, sizeof(msgbuf), fmt, ap);
	va_end(ap);

	print_context_msg(s, labelbuf, msgbuf);
}
