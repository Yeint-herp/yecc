#ifndef LEX_STRING_CONCAT_H
#define LEX_STRING_CONCAT_H

#include <base/vector.h>
#include <context/context.h>
#include <diag/diag.h>
#include <lex/token.h>
#include <stdbool.h>
#include <stddef.h>

enum lit_kind { LIT_PLAIN = 0, LIT_UTF8 = 1, LIT_UTF16 = 2, LIT_UTF32 = 3, LIT_WIDE = 4 };

static inline bool token_is_string_lit(const struct token *t) { return t && t->kind == TOKEN_STRING_LITERAL; }

/* Concatenate exactly two cooked string-literal tokens into *out.
   - Uses C rules for prefix promotion (plain/u8/u/U/L).
   - Emits width-promotion diagnostics.
   - span_hint should cover a.loc.start to b.loc.end.
   Returns false if either input isn't a string literal. */
bool lex_concat_string_pair(struct yecc_context *ctx, const struct token *a, const struct token *b,
							struct source_span span_hint, struct token *out);

/* In-place pass over a token vector: collapses any run of adjacent string literals. */
void lex_concat_adjacent_string_literals(struct yecc_context *ctx,
										 void *tokens); /* tokens = vector_of(struct token) * */

#endif /* LEX_STRING_CONCAT_H */
