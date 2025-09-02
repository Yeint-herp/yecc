#ifndef LEXER_H
#define LEXER_H

#include <base/streamer.h>
#include <base/string_intern.h>
#include <base/vector.h>
#include <context/context.h>
#include <diag/diag.h>
#include <lex/token.h>
#include <stdbool.h>

enum yecc_pp_kind {
	YECC_PP_NONE = 0,
	YECC_PP_INCLUDE,
	YECC_PP_INCLUDE_NEXT,
	YECC_PP_IMPORT,
	YECC_PP_EMBED,
	YECC_PP_OTHER
};

struct lexer {
	struct streamer s;
	bool at_line_start;
	bool in_directive;
	struct yecc_context *ctx;

	enum yecc_pp_kind pp_kind;
	bool expect_header_name;
};

/**
 * Initialize the lexer on the given source file.
 * Returns true on success, false on failure.
 */
bool lexer_init(struct lexer *lx, const char *filename, struct yecc_context *ctx);

/* destroy the lexer, freeing any internal resources */
void lexer_destroy(struct lexer *lx);

/**
 * Read and return the next token from the input.
 * Skips whitespace and comments, recovers on errors, and
 * reports diagnostics via the diag module.
 */
struct token lexer_next(struct lexer *lx);

#endif /* LEXER_H */
