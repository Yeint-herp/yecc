#ifndef PRINT_H
#define PRINT_H

#include <base/streamer.h>
#include <lex/token.h>
#include <stdio.h>
#include <string.h>

const char *token_kind_name(enum token_kind k);

const char *int_base_name(int b);
const char *float_style_name(int s);
const char *float_suf_name(int s);

void dump_flags(unsigned f, char *out, size_t n);

void dump_span(const struct source_span *sp);

void dump_token(const struct token *t, const char *label);

#endif /* PRINT_H */
