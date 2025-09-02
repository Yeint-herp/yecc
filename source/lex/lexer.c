#include <base/streamer.h>
#include <base/string_intern.h>
#include <base/vector.h>
#include <context/context.h>
#include <ctype.h>
#include <diag/diag.h>
#include <errno.h>
#include <lex/lexer.h>
#include <lex/token.h>
#include <limits.h>
#include <locale.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <uchar.h>
#include <wchar.h>

static void diag_extension(struct lexer *lx, struct source_span sp, const char *fmt, ...) {
	if (!lx->ctx->pedantic)
		return;
	if (!yecc_context_warning_enabled(lx->ctx, YECC_W_PEDANTIC))
		return;

	va_list ap;
	va_start(ap, fmt);
	if (lx->ctx->warnings_as_errors && yecc_context_warning_as_error(lx->ctx, YECC_W_PEDANTIC))
		diag_errorv(sp, fmt, ap);
	else
		diag_warningv(sp, fmt, ap);
	va_end(ap);
}

static void diag_alt_token(struct lexer *lx, struct source_span sp, const char *kind, const char *lexeme) {
	if (!lx->ctx->enable_trigraphs) {
		if (yecc_context_warning_as_error(lx->ctx, YECC_W_TRIGRAPHS) || lx->ctx->warnings_as_errors)
			diag_error(sp, "%s '%s' used, but alternative tokens are disabled", kind, lexeme);
		else
			diag_warning(sp, "%s '%s' used, but alternative tokens are ignored", kind, lexeme);
		return;
	}

	if (!yecc_context_warning_enabled(lx->ctx, YECC_W_TRIGRAPHS))
		return;

	if (yecc_context_warning_as_error(lx->ctx, YECC_W_TRIGRAPHS) || lx->ctx->warnings_as_errors)
		diag_error(sp, "%s '%s' translated", kind, lexeme);
	else
		diag_warning(sp, "%s '%s' translated", kind, lexeme);
}

static bool utf8_decode_one(struct streamer *s, uint32_t *out_cp) {
	int first = streamer_peek(s);
	if (first < 0)
		return false;

	unsigned char b0 = (unsigned char)first;
	size_t need = 0;
	uint32_t cp = 0;

	if (b0 <= 0x7F) {
		need = 1;
		cp = b0;
	} else if ((b0 & 0xE0) == 0xC0) {
		need = 2;
		cp = b0 & 0x1F;
	} else if ((b0 & 0xF0) == 0xE0) {
		need = 3;
		cp = b0 & 0x0F;
	} else if ((b0 & 0xF8) == 0xF0) {
		need = 4;
		cp = b0 & 0x07;
	} else {
		struct source_position p = streamer_position(s);
		diag_error((struct source_span){p, p}, "invalid UTF-8 start byte 0x%02X", b0);
		streamer_next(s);
		return false;
	}

	uint8_t tmp[4];
	for (size_t i = 0; i < need; i++) {
		if (streamer_eof(s)) {
			struct source_position p = streamer_position(s);
			diag_error((struct source_span){p, p}, "truncated UTF-8 sequence");
			return false;
		}
		int c = streamer_peek(s);
		if (i > 0 && ((c & 0xC0) != 0x80)) {
			struct source_position p = streamer_position(s);
			diag_error((struct source_span){p, p}, "invalid UTF-8 continuation byte 0x%02X", (unsigned char)c);
			streamer_next(s);
			return false;
		}
		tmp[i] = (uint8_t)streamer_next(s);
	}

	for (size_t i = 1; i < need; i++)
		cp = (cp << 6) | (tmp[i] & 0x3F);

	if ((need == 2 && cp < 0x80) || (need == 3 && cp < 0x800) || (need == 4 && cp < 0x10000) ||
		(cp >= 0xD800 && cp <= 0xDFFF) || (cp > 0x10FFFF)) {
		struct source_position p = streamer_position(s);
		diag_error((struct source_span){p, p}, "invalid UTF-8 code point U+%04X", (unsigned)cp);
		*out_cp = 0xFFFD;
		return true;
	}

	*out_cp = cp;
	return true;
}

enum lit_kind { LIT_PLAIN = 0, LIT_UTF8 = 1, LIT_UTF16 = 2, LIT_UTF32 = 3, LIT_WIDE = 4 };

struct lit_info {
	enum lit_kind kind;
	unsigned rank;
	const char *name;
	unsigned unit_bits;
};

static inline struct lit_info lit_info_of(enum lit_kind k, const struct yecc_context *ctx) {
	switch (k) {
	case LIT_PLAIN:
		return (struct lit_info){k, 0, "plain", 8};
	case LIT_UTF8:
		return (struct lit_info){k, 1, "u8", 8};
	case LIT_UTF16:
		return (struct lit_info){k, 2, "u", 16};
	case LIT_UTF32:
		return (struct lit_info){k, 3, "U", 32};
	case LIT_WIDE:
		return (struct lit_info){k, 4, "L", (ctx && ctx->wchar_bits ? ctx->wchar_bits : 32)};
	}
	return (struct lit_info){LIT_PLAIN, 0, "plain", 8};
}

static inline enum lit_kind lit_promote(enum lit_kind a, enum lit_kind b, const struct yecc_context *ctx,
										bool *out_widened) {
	struct lit_info A = lit_info_of(a, ctx), B = lit_info_of(b, ctx);
	if (out_widened)
		*out_widened = (A.rank != (A.rank > B.rank ? A.rank : B.rank));
	return (A.rank >= B.rank) ? a : b;
}

static inline void diag_promotion(struct lexer *lx, struct source_span sp, enum lit_kind from, enum lit_kind to) {
	if (from == to)
		return;
	if (!yecc_context_warning_enabled(lx->ctx, YECC_W_STRING_WIDTH_PROMOTION))
		return;
	if (lx->ctx->warnings_as_errors && yecc_context_warning_as_error(lx->ctx, YECC_W_STRING_WIDTH_PROMOTION))
		diag_error(sp, "string literal concatenation promotes from %s to %s", lit_info_of(from, lx->ctx).name,
				   lit_info_of(to, lx->ctx).name);
	else
		diag_warning(sp, "string literal concatenation promotes from %s to %s", lit_info_of(from, lx->ctx).name,
					 lit_info_of(to, lx->ctx).name);
}

static inline uint32_t target_wchar_max(const struct yecc_context *ctx) {
	if (!ctx)
		return 0;

	switch (ctx->wchar_bits) {
	case 8:
		return 0xFF;
	case 16:
		return 0xFFFF;
	case 32:
		return 0x7FFFFFFF;
	}
	return 0;
}

static void skip_line_splice(struct lexer *lx) {
	for (;;) {
		struct streamer_blob blob = streamer_get_blob(&lx->s);
		if (blob.cache[2] == '\\' && ((blob.cache[3] == '\n') || (blob.cache[3] == '\r' && blob.cache[4] == '\n'))) {
			streamer_next(&lx->s);
			streamer_next(&lx->s);
			if (blob.cache[3] == '\r')
				streamer_next(&lx->s);
			continue;
		}
		return;
	}
}

enum alt_kind { ALT_TRI, ALT_DI };

struct alt_entry {
	const char *pat;
	const char *rep;
	enum alt_kind kind;
};

static const struct alt_entry ALT_TRI_TABLE[] = {
	{"??=", "#", ALT_TRI}, {"??/", "\\", ALT_TRI}, {"??'", "^", ALT_TRI}, {"??(", "[", ALT_TRI}, {"??)", "]", ALT_TRI},
	{"??!", "|", ALT_TRI}, {"??<", "{", ALT_TRI},  {"??>", "}", ALT_TRI}, {"??-", "~", ALT_TRI},
};

static const struct alt_entry ALT_DI_TABLE[] = {
	{"<:", "[", ALT_DI}, {":>", "]", ALT_DI}, {"<%", "{", ALT_DI},
	{"%>", "}", ALT_DI}, {"%:", "#", ALT_DI}, {"%:%:", "##", ALT_DI},
};

static int try_trigraph(struct lexer *lx, int *out) {
	struct streamer *s = &lx->s;

	struct source_position startp = streamer_position(s);
	int a = streamer_peek(s);
	if (a != '?')
		return 0;
	streamer_next(s);
	int b = streamer_peek(s);
	if (b != '?') {
		streamer_unget(s);
		return 0;
	}
	streamer_next(s);
	int c = streamer_peek(s);
	if (c < 0) {
		streamer_unget(s);
		streamer_unget(s);
		return 0;
	}

	char buf[4] = {'?', '?', (char)c, 0};

	const struct alt_entry *hit = nullptr;
	for (size_t i = 0; i < sizeof(ALT_TRI_TABLE) / sizeof(ALT_TRI_TABLE[0]); ++i) {
		if (ALT_TRI_TABLE[i].pat[2] == buf[2]) {
			hit = &ALT_TRI_TABLE[i];
			break;
		}
	}

	if (!hit) {
		streamer_unget(s);
		streamer_unget(s);
		return 0;
	}

	struct source_position endp = streamer_position(s);
	streamer_next(s);

	struct source_span sp = {startp, endp};
	if (!lx->ctx->enable_trigraphs) {
		diag_alt_token(lx, sp, "trigraph", buf);
		streamer_unget(s);
		streamer_unget(s);
		streamer_unget(s);
		return 0;
	}

	diag_alt_token(lx, sp, "trigraph", buf);
	*out = (unsigned char)hit->rep[0];
	return 1;
}

static int streamer_next_preproc(struct lexer *lx) {
	for (;;) {
		struct streamer_blob blob = streamer_get_blob(&lx->s);
		if (blob.cache[2] == '\\' && ((blob.cache[3] == '\n') || (blob.cache[3] == '\r' && blob.cache[4] == '\n'))) {
			streamer_next(&lx->s);
			streamer_next(&lx->s);
			if (blob.cache[3] == '\r')
				streamer_next(&lx->s);
			continue;
		}
		break;
	}

	{
		int mapped = 0;
		if (try_trigraph(lx, &mapped))
			return mapped;
	}

	return streamer_next(&lx->s);
}

#define NEXT(lx) streamer_next_preproc(lx)
#define PEEK(lx) streamer_peek(&(lx)->s)

static void skip_pp_hspace(struct lexer *lx) {
	for (;;) {
		struct streamer_blob b = streamer_get_blob(&lx->s);
		if (b.cache[2] == '\\' && b.cache[3] == '\n') {
			streamer_next(&lx->s);
			streamer_next(&lx->s);
			continue;
		}
		if (b.cache[2] == '\\' && b.cache[3] == '\r' && b.cache[4] == '\n') {
			streamer_next(&lx->s);
			streamer_next(&lx->s);
			streamer_next(&lx->s);
			continue;
		}

		int c = streamer_peek(&lx->s);
		if (c == ' ' || c == '\t' || c == '\v' || c == '\f') {
			streamer_next(&lx->s);
			continue;
		}
		break;
	}
}

static size_t peek_preproc(struct lexer *lx, char out[], size_t need) {
	if (need == 0)
		return 0;

	struct streamer *s = &lx->s;
	struct source_position saved = streamer_position(s);

	size_t got = 0;

	while (got < need) {
		int a = streamer_peek(s);
		if (a < 0)
			break;

		if (a == '\\') {
			(void)streamer_next(s);
			int b = streamer_peek(s);
			if (b == '\n') {
				(void)streamer_next(s);
				continue;
			}
			if (b == '\r') {
				(void)streamer_next(s);
				int c = streamer_peek(s);
				if (c == '\n') {
					(void)streamer_next(s);
					continue;
				}
				(void)streamer_unget(s);
			}
			out[got++] = '\\';
			continue;
		}

		if (lx->ctx->enable_trigraphs && a == '?') {
			(void)streamer_next(s);
			int b = streamer_peek(s);
			if (b == '?') {
				(void)streamer_next(s);
				int c = streamer_peek(s);
				if (c >= 0) {
					int matched = 0;
					for (size_t i = 0; i < sizeof(ALT_TRI_TABLE) / sizeof(ALT_TRI_TABLE[0]); ++i) {
						if (ALT_TRI_TABLE[i].pat[2] == (char)c) {
							(void)streamer_next(s);
							out[got++] = ALT_TRI_TABLE[i].rep[0];
							matched = 1;
							break;
						}
					}
					if (!matched) {
						(void)streamer_unget(s);
						(void)streamer_unget(s);
						out[got++] = '?';
					}
					continue;
				} else {
					(void)streamer_unget(s);
				}
			}
			out[got++] = '?';
			continue;
		}

		(void)streamer_next(s);
		out[got++] = (char)a;
	}

	(void)streamer_seek(s, saved.offset);
	return got;
}

static void skip_to_safe_point(struct lexer *lx) {
	bool ended_on_nl = false;
	for (;;) {
		int c = NEXT(lx);
		if (c < 0)
			break;
		if (c == '\n') {
			ended_on_nl = true;
			break;
		}
		if (c == ';')
			break;
	}
	lx->at_line_start = ended_on_nl || PEEK(lx) == '\n';
	lx->in_directive = false;
}

static bool utf8_validate_and_append(void *buf, struct streamer *s) {
	int first = streamer_peek(s);
	if (first < 0) {
		struct source_position p = streamer_position(s);
		diag_error((struct source_span){p, p}, "unexpected end of file in UTF-8 sequence");
		return false;
	}

	size_t len = 0;
	if ((unsigned)first <= 0x7F) {
		len = 1;
	} else if ((first & 0xE0) == 0xC0) {
		len = 2;
	} else if ((first & 0xF0) == 0xE0) {
		len = 3;
	} else if ((first & 0xF8) == 0xF0) {
		len = 4;
	} else {
		struct source_position p = streamer_position(s);
		diag_error((struct source_span){p, p}, "invalid UTF-8 start byte 0x%02X", (unsigned char)first);
		streamer_next(s);
		return false;
	}
	uint8_t tmp[4];
	for (size_t i = 0; i < len; i++) {
		if (streamer_eof(s)) {
			struct source_position p = streamer_position(s);
			diag_error((struct source_span){p, p}, "truncated UTF-8 sequence");
			return false;
		}
		int c = streamer_peek(s);
		if (i > 0 && (c & 0xC0) != 0x80) {
			struct source_position p = streamer_position(s);
			diag_error((struct source_span){p, p}, "invalid UTF-8 continuation byte 0x%02X", (unsigned char)c);
			return false;
		}
		tmp[i] = (uint8_t)streamer_next(s);
	}
	vector_of(char) *vec = buf;

	for (size_t i = 0; i < len; i++)
		vector_push(vec, tmp[i]);
	return true;
}

static uint32_t parse_ucn(struct lexer *lx) {
	NEXT(lx);
	int kind = NEXT(lx);
	int count = (kind == 'u' ? 4 : 8);
	uint32_t code = 0;
	for (int i = 0; i < count; i++) {
		int d = streamer_peek(&lx->s);
		if (!isxdigit((unsigned char)d)) {
			struct source_position p = streamer_position(&lx->s);
			diag_error((struct source_span){p, p}, "invalid UCN in identifier");
			return 0xFFFD;
		}
		char ch = (char)NEXT(lx);
		code = (code << 4) + (isdigit((unsigned char)ch) ? ch - '0' : toupper((unsigned char)ch) - 'A' + 10);
	}
	if (code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) {
		struct source_position p = streamer_position(&lx->s);
		diag_error((struct source_span){p, p}, "invalid Unicode code point U+%04X", code);
		return 0xFFFD;
	}
	return code;
}

static const struct {
	const char *str;
	enum token_kind kind;
} puncts[] = {
	{"<<=", TOKEN_LSHIFT_ASSIGN},
	{">>=", TOKEN_RSHIFT_ASSIGN},
	{"...", TOKEN_ELLIPSIS},

	{"##", TOKEN_PP_HASHHASH},
	{"<<", TOKEN_LSHIFT},
	{">>", TOKEN_RSHIFT},
	{"&&", TOKEN_AND_AND},
	{"||", TOKEN_OR_OR},
	{"->", TOKEN_ARROW},
	{"++", TOKEN_PLUS_PLUS},
	{"--", TOKEN_MINUS_MINUS},
	{"+=", TOKEN_PLUS_ASSIGN},
	{"-=", TOKEN_MINUS_ASSIGN},
	{"*=", TOKEN_STAR_ASSIGN},
	{"/=", TOKEN_SLASH_ASSIGN},
	{"%=", TOKEN_PERCENT_ASSIGN},
	{"&=", TOKEN_AMP_ASSIGN},
	{"^=", TOKEN_XOR_ASSIGN},
	{"|=", TOKEN_OR_ASSIGN},
	{"<=", TOKEN_LE},
	{">=", TOKEN_GE},
	{"==", TOKEN_EQEQ},
	{"!=", TOKEN_NEQ},

	{"#", TOKEN_PP_HASH},
	{"?", TOKEN_QUESTION},
	{":", TOKEN_COLON},
	{";", TOKEN_SEMICOLON},
	{",", TOKEN_COMMA},
	{".", TOKEN_PERIOD},
	{"+", TOKEN_PLUS},
	{"-", TOKEN_MINUS},
	{"*", TOKEN_STAR},
	{"/", TOKEN_SLASH},
	{"%", TOKEN_PERCENT},
	{"<", TOKEN_LT},
	{">", TOKEN_GT},
	{"=", TOKEN_ASSIGN},
	{"!", TOKEN_EXCLAMATION},
	{"~", TOKEN_TILDE},
	{"^", TOKEN_CARET},
	{"&", TOKEN_AMP},
	{"|", TOKEN_PIPE},
	{"(", TOKEN_LPAREN},
	{")", TOKEN_RPAREN},
	{"[", TOKEN_LBRACKET},
	{"]", TOKEN_RBRACKET},
	{"{", TOKEN_LBRACE},
	{"}", TOKEN_RBRACE},
};

#ifndef KW_DB
#define KW_DB(X)                                                                                                       \
	X("__asm__", TOKEN_KW___ASM__, 0, 0, 1, 0, 0)                                                                      \
	X("asm", TOKEN_KW_ASM, 0, 0, 1, 0, 0)                                                                              \
	X("typeof", TOKEN_KW_TYPEOF, 0, 0, 1, 0, 0)                                                                        \
	X("__attribute__", TOKEN_KW___ATTRIBUTE__, 0, 0, 1, 0, 0)                                                          \
	X("__builtin_types_compatible_p", TOKEN_KW___BUILTIN_TYPES_COMPATIBLE_P, 0, 0, 1, 0, 0)                            \
	X("__auto_type", TOKEN_KW___AUTO_TYPE, 0, 0, 1, 0, 0)                                                              \
	X("__extension__", TOKEN_KW___EXTENSION__, 0, 0, 1, 0, 0)                                                          \
	X("__label__", TOKEN_KW___LABEL__, 0, 0, 1, 0, 0)                                                                  \
	X("__real__", TOKEN_KW___REAL__, 0, 0, 1, 0, 0)                                                                    \
	X("__imag__", TOKEN_KW___IMAG__, 0, 0, 1, 0, 0)                                                                    \
	X("__thread", TOKEN_KW___THREAD, 0, 0, 1, 0, 0)                                                                    \
	X("__FUNCTION__", TOKEN_KW___FUNCTION__, 0, 0, 1, 0, 0)                                                            \
	X("__int128", TOKEN_KW___INT128, 0, 0, 1, 0, 0)                                                                    \
	X("__const__", TOKEN_KW___CONST__, 0, 0, 1, 0, 0)                                                                  \
	X("__signed__", TOKEN_KW___SIGNED__, 0, 0, 1, 0, 0)                                                                \
	X("__inline__", TOKEN_KW___INLINE__, 0, 0, 1, 0, 0)                                                                \
	X("__restrict__", TOKEN_KW___RESTRICT__, 0, 0, 1, 0, 0)                                                            \
	X("__volatile__", TOKEN_KW___VOLATILE__, 0, 0, 1, 0, 0)                                                            \
	X("auto", TOKEN_KW_AUTO, 0, 0, 0, 0, 0)                                                                            \
	X("typedef", TOKEN_KW_TYPEDEF, 0, 0, 0, 0, 0)                                                                      \
	X("break", TOKEN_KW_BREAK, 0, 0, 0, 0, 0)                                                                          \
	X("case", TOKEN_KW_CASE, 0, 0, 0, 0, 0)                                                                            \
	X("char", TOKEN_KW_CHAR, 0, 0, 0, 0, 0)                                                                            \
	X("const", TOKEN_KW_CONST, 0, 0, 0, 0, 0)                                                                          \
	X("continue", TOKEN_KW_CONTINUE, 0, 0, 0, 0, 0)                                                                    \
	X("default", TOKEN_KW_DEFAULT, 0, 0, 0, 0, 0)                                                                      \
	X("do", TOKEN_KW_DO, 0, 0, 0, 0, 0)                                                                                \
	X("double", TOKEN_KW_DOUBLE, 0, 0, 0, 0, 0)                                                                        \
	X("enum", TOKEN_KW_ENUM, 0, 0, 0, 0, 0)                                                                            \
	X("extern", TOKEN_KW_EXTERN, 0, 0, 0, 0, 0)                                                                        \
	X("float", TOKEN_KW_FLOAT, 0, 0, 0, 0, 0)                                                                          \
	X("for", TOKEN_KW_FOR, 0, 0, 0, 0, 0)                                                                              \
	X("goto", TOKEN_KW_GOTO, 0, 0, 0, 0, 0)                                                                            \
	X("inline", TOKEN_KW_INLINE, 0, 1, 0, 0, 0)                                                                        \
	X("int", TOKEN_KW_INT, 0, 0, 0, 0, 0)                                                                              \
	X("long", TOKEN_KW_LONG, 0, 0, 0, 0, 0)                                                                            \
	X("register", TOKEN_KW_REGISTER, 0, 0, 0, 0, 1)                                                                    \
	X("restrict", TOKEN_KW_RESTRICT, 0, 1, 0, 0, 0)                                                                    \
	X("return", TOKEN_KW_RETURN, 0, 0, 0, 0, 0)                                                                        \
	X("short", TOKEN_KW_SHORT, 0, 0, 0, 0, 0)                                                                          \
	X("signed", TOKEN_KW_SIGNED, 0, 0, 0, 0, 0)                                                                        \
	X("sizeof", TOKEN_KW_SIZEOF, 0, 0, 0, 0, 0)                                                                        \
	X("static", TOKEN_KW_STATIC, 0, 0, 0, 0, 0)                                                                        \
	X("struct", TOKEN_KW_STRUCT, 0, 0, 0, 0, 0)                                                                        \
	X("switch", TOKEN_KW_SWITCH, 0, 0, 0, 0, 0)                                                                        \
	X("union", TOKEN_KW_UNION, 0, 0, 0, 0, 0)                                                                          \
	X("unsigned", TOKEN_KW_UNSIGNED, 0, 0, 0, 0, 0)                                                                    \
	X("void", TOKEN_KW_VOID, 0, 0, 0, 0, 0)                                                                            \
	X("volatile", TOKEN_KW_VOLATILE, 0, 0, 0, 0, 0)                                                                    \
	X("while", TOKEN_KW_WHILE, 0, 0, 0, 0, 0)                                                                          \
	X("_Bool", TOKEN_KW__BOOL, 0, 1, 0, 0, 0)                                                                          \
	X("_Complex", TOKEN_KW__COMPLEX, 0, 1, 0, 0, 0)                                                                    \
	X("_Imaginary", TOKEN_KW_IMAGINARY, 0, 1, 0, 0, 2)                                                                 \
	X("_Static_assert", TOKEN_KW__STATIC_ASSERT, 0, 2, 0, 1, 0)                                                        \
	X("_Noreturn", TOKEN_KW_NORETURN, 0, 2, 0, 0, 1)                                                                   \
	X("_Generic", TOKEN_KW_GENERIC, 0, 2, 0, 0, 0)                                                                     \
	X("_Atomic", TOKEN_KW_ATOMIC, 0, 2, 0, 0, 0)                                                                       \
	X("static_assert", TOKEN_KW__STATIC_ASSERT, 0, 3, 0, 2, 0)                                                         \
	X("elifdef", TOKEN_PP_ELIFDEF, 1, 3, 0, 0, 0)                                                                      \
	X("elifndef", TOKEN_PP_ELIFNDEF, 1, 3, 0, 0, 0)                                                                    \
	X("alignas", TOKEN_KW_ALIGNAS, 0, 3, 0, 2, 0)                                                                      \
	X("alignof", TOKEN_KW_ALIGNOF, 0, 3, 0, 2, 0)                                                                      \
	X("thread_local", TOKEN_KW_THREAD_LOCAL, 0, 3, 0, 2, 0)                                                            \
	X("bool", TOKEN_KW_BOOL, 0, 3, 0, 0, 0)                                                                            \
	X("true", TOKEN_KW_TRUE, 0, 3, 0, 0, 0)                                                                            \
	X("false", TOKEN_KW_FALSE, 0, 3, 0, 0, 0)                                                                          \
	X("_BitInt", TOKEN_KW__BITINT, 0, 3, 0, 0, 0)                                                                      \
	X("_Decimal32", TOKEN_KW__DECIMAL32, 0, 3, 0, 0, 0)                                                                \
	X("_Decimal64", TOKEN_KW__DECIMAL64, 0, 3, 0, 0, 0)                                                                \
	X("_Decimal128", TOKEN_KW__DECIMAL128, 0, 3, 0, 0, 0)                                                              \
	X("_Float32", TOKEN_KW__FLOAT32, 0, 3, 0, 0, 0)                                                                    \
	X("_Float64", TOKEN_KW__FLOAT64, 0, 3, 0, 0, 0)                                                                    \
	X("_Float80", TOKEN_KW__FLOAT80, 0, 3, 0, 0, 0)                                                                    \
	X("_Float128", TOKEN_KW__FLOAT128, 0, 3, 0, 0, 0)                                                                  \
	X("_Alignas", TOKEN_KW_ALIGNAS, 0, 2, 0, 1, 0)                                                                     \
	X("_Alignof", TOKEN_KW_ALIGNOF, 0, 2, 0, 1, 0)                                                                     \
	X("_Thread_local", TOKEN_KW_THREAD_LOCAL, 0, 2, 0, 1, 0)                                                           \
	X("defined", TOKEN_PP_DEFINED, 1, 0, 0, 0, 0)                                                                      \
	X("include", TOKEN_PP_INCLUDE, 1, 0, 0, 0, 0)                                                                      \
	X("define", TOKEN_PP_DEFINE, 1, 0, 0, 0, 0)                                                                        \
	X("undef", TOKEN_PP_UNDEF, 1, 0, 0, 0, 0)                                                                          \
	X("if", TOKEN_PP_IF, 1, 0, 0, 0, 0)                                                                                \
	X("ifdef", TOKEN_PP_IFDEF, 1, 0, 0, 0, 0)                                                                          \
	X("ifndef", TOKEN_PP_IFNDEF, 1, 0, 0, 0, 0)                                                                        \
	X("elif", TOKEN_PP_ELIF, 1, 0, 0, 0, 0)                                                                            \
	X("else", TOKEN_PP_ELSE, 1, 0, 0, 0, 0)                                                                            \
	X("if", TOKEN_KW_IF, 0, 0, 0, 0, 0)                                                                                \
	X("else", TOKEN_KW_ELSE, 0, 0, 0, 0, 0)                                                                            \
	X("endif", TOKEN_PP_ENDIF, 1, 0, 0, 0, 0)                                                                          \
	X("error", TOKEN_PP_ERROR, 1, 0, 0, 0, 0)                                                                          \
	X("line", TOKEN_PP_LINE, 1, 0, 0, 0, 0)                                                                            \
	X("pragma", TOKEN_PP_PRAGMA, 1, 0, 0, 0, 0)                                                                        \
	X("_Pragma", TOKEN_KW__PRAGMA, 0, 1, 0, 0, 0)                                                                      \
	X("warning", TOKEN_PP_WARNING, 1, 3, 0, 0, 0)                                                                      \
	X("embed", TOKEN_PP_EMBED, 1, 3, 0, 0, 0)                                                                          \
	X("__has_include", TOKEN_PP___HAS_INCLUDE, 1, 3, 0, 0, 0)                                                          \
	X("__has_c_attribute", TOKEN_PP___HAS_C_ATTRIBUTE, 1, 3, 0, 0, 0)                                                  \
	X("__assert", TOKEN_PP__ASSERT, 1, 0, 0, 0, 0)                                                                     \
	X("__assert_any", TOKEN_PP__ASSERT_ANY, 1, 0, 0, 0, 0)                                                             \
	X("__VA_OPT__", TOKEN_PP___VA_OPT__, 1, 3, 0, 0, 0)                                                                \
	X("include_next", TOKEN_PP_INCLUDE_NEXT, 1, 0, 1, 0, 0)                                                            \
	X("import", TOKEN_PP_IMPORT, 1, 0, 1, 0, 0)                                                                        \
	X("ident", TOKEN_PP_IDENT, 1, 0, 1, 0, 0)                                                                          \
	X("sccs", TOKEN_PP_SCCS, 1, 0, 1, 0, 0)                                                                            \
	X("assert", TOKEN_PP_ASSERT, 1, 0, 1, 0, 0)                                                                        \
	X("unassert", TOKEN_PP_UNASSERT, 1, 0, 1, 0, 0)
#endif

enum kw_spelling_form : unsigned { KW_SPELLING_NEUTRAL = 0, KW_SPELLING_OLD_FORM = 1, KW_SPELLING_NEW_FORM = 2 };
enum c23_status : unsigned { C23_NONE = 0, C23_DEPRECATED = 1, C23_REMOVED = 2 };

struct kw_entry {
	const char *name;
	enum token_kind kind;
	bool is_pp;
	unsigned min_std; // 0=C90, 1=C99, 2=C11, 3=C23
	bool gnu_only;
	enum kw_spelling_form spelling_form;
	enum c23_status c23_status;
};

#define MAKE_ENTRY(n, k, pp, min, gnu, spell, c23s)                                                                    \
	{(n), (k), (pp) != (0), (unsigned)(min), (gnu) != (0), (unsigned)(spell), (unsigned)(c23s)},

static const struct kw_entry KW_TABLE[] = {KW_DB(MAKE_ENTRY)};

static const struct kw_entry *kw_lookup_first(const char *s) {
	for (size_t i = 0; i < sizeof(KW_TABLE) / sizeof(KW_TABLE[0]); ++i)
		if (strcmp(s, KW_TABLE[i].name) == 0)
			return &KW_TABLE[i];
	return nullptr;
}

static const struct kw_entry *kw_lookup_next(const char *s, const struct kw_entry *prev) {
	size_t start = 0;
	if (prev) {
		if (prev < &KW_TABLE[0] || prev >= &KW_TABLE[0] + (sizeof(KW_TABLE) / sizeof(KW_TABLE[0])))
			return nullptr;
		start = (size_t)((prev - &KW_TABLE[0]) + 1);
	}
	for (size_t i = start; i < sizeof(KW_TABLE) / sizeof(KW_TABLE[0]); ++i)
		if (strcmp(s, KW_TABLE[i].name) == 0)
			return &KW_TABLE[i];
	return nullptr;
}

static inline bool std_at_least(struct yecc_context *ctx, unsigned min_std) {
	unsigned cur = yecc_std_at_least(ctx, YECC_LANG_C23)   ? 3
				   : yecc_std_at_least(ctx, YECC_LANG_C11) ? 2
				   : yecc_std_at_least(ctx, YECC_LANG_C99) ? 1
														   : 0;
	return cur >= min_std;
}

static void maybe_warn_from_entry(struct lexer *lx, const char *lexeme, const struct kw_entry *E,
								  struct source_span sp) {
	const bool gnu = lx->ctx->gnu_extensions;

	if (E->gnu_only && !gnu) {
		if (E->is_pp)
			diag_extension(lx, sp, "non-standard preprocessor directive/operator '%s' used in non-GNU mode", lexeme);
		else
			diag_extension(lx, sp, "GNU extension keyword '%s' used in non-GNU mode", lexeme);
		return;
	}

	if (!std_at_least(lx->ctx, E->min_std) && !gnu) {
		const char *need = (E->min_std == 1) ? "C99" : (E->min_std == 2) ? "C11" : "C23";
		if (E->is_pp)
			diag_extension(lx, sp, "preprocessor directive/operator '%s' requires %s or GNU extensions", lexeme, need);
		else
			diag_extension(lx, sp, "keyword '%s' requires %s or GNU extensions", lexeme, need);
	}

	if (!E->is_pp) {
		if (E->spelling_form == KW_SPELLING_NEW_FORM && !std_at_least(lx->ctx, 3) && !gnu) {
			diag_extension(lx, sp, "keyword '%s' is the C23 spelling; requires C23 or GNU extensions", lexeme);
		} else if (E->spelling_form == KW_SPELLING_OLD_FORM && std_at_least(lx->ctx, 3) && !gnu) {
			diag_warning(sp, "C23 deprecates the underscored spelling '%s'; prefer the C23 spelling", lexeme);
		}
	}

	if (!E->is_pp && std_at_least(lx->ctx, 3) && !gnu) {
		if (E->c23_status == C23_REMOVED)
			diag_error(sp, "C23 removed the '%s' keyword", lexeme);
		else if (E->c23_status == C23_DEPRECATED)
			diag_warning(sp, "C23 deprecates '%s'; prefer attributes or newer forms where applicable", lexeme);
	}
}

static inline bool is_include_like(enum token_kind k) {
	return (k == TOKEN_PP_INCLUDE || k == TOKEN_PP_INCLUDE_NEXT || k == TOKEN_PP_IMPORT);
}
static inline bool is_embed_kw(enum token_kind k) { return (k == TOKEN_PP_EMBED); }

static inline void pp_note_after_keyword(struct lexer *lx, enum token_kind k) {
	if (!lx->in_directive)
		return;
	if (is_include_like(k)) {
		lx->pp_kind = (k == TOKEN_PP_INCLUDE)		 ? YECC_PP_INCLUDE
					  : (k == TOKEN_PP_INCLUDE_NEXT) ? YECC_PP_INCLUDE_NEXT
													 : YECC_PP_IMPORT;
		lx->expect_header_name = true;
	} else if (is_embed_kw(k)) {
		lx->pp_kind = YECC_PP_EMBED;
		lx->expect_header_name = true;
	} else {
		lx->pp_kind = YECC_PP_OTHER;
		lx->expect_header_name = false;
	}
}

static inline const struct kw_entry *kw_lookup_ctx(const char *s, bool in_directive) {
	const struct kw_entry *first = kw_lookup_first(s);
	if (!first)
		return nullptr;

	const struct kw_entry *best = nullptr;
	for (const struct kw_entry *E = first; E; E = kw_lookup_next(s, E)) {
		if (!best)
			best = E;
		if (in_directive) {
			if (E->is_pp)
				return E;
		} else {
			if (!E->is_pp)
				return E;
		}
	}
	return best;
}

enum token_kind classify_ident(const char *s, bool in_directive) {
	const struct kw_entry *E = kw_lookup_ctx(s, in_directive);
	if (!E)
		return TOKEN_IDENTIFIER;

	if (E->is_pp && !in_directive)
		return TOKEN_IDENTIFIER;

	return E->kind;
}

void maybe_warn_ident_use(struct lexer *lx, const char *lexeme, enum token_kind k, struct source_span sp) {
	(void)k;
	const struct kw_entry *E = kw_lookup_ctx(lexeme, lx->in_directive);
	if (!E)
		return;

	maybe_warn_from_entry(lx, lexeme, E, sp);
	if (E->is_pp)
		pp_note_after_keyword(lx, E->kind);
}

static void skip_space_and_comments(struct lexer *lx) {
	for (;;) {
		skip_line_splice(lx);

		while (!streamer_eof(&lx->s) && isspace((unsigned char)PEEK(lx))) {
			if (NEXT(lx) == '\n') {
				lx->at_line_start = true;
				lx->in_directive = false;
			}
		}
		if (PEEK(lx) == '/') {
			struct streamer_blob blob = streamer_get_blob(&lx->s);
			if (blob.cache[2] == '/' && blob.cache[3] == '/') {
				if (!yecc_std_at_least(lx->ctx, YECC_LANG_C99) && !(lx->ctx->gnu_extensions)) {
					struct source_position p = streamer_position(&lx->s);
					diag_extension(lx, (struct source_span){p, p},
								   "C89 mode: '//' comments are a non-standard extension");
				}
				NEXT(lx);
				NEXT(lx);
				while (!streamer_eof(&lx->s) && NEXT(lx) != '\n')
					;
				lx->at_line_start = true;
				lx->in_directive = false;
				continue;
			}
			if (blob.cache[2] == '/' && blob.cache[3] == '*') {
				NEXT(lx);
				NEXT(lx);
				bool closed = false;
				while (!streamer_eof(&lx->s)) {
					int d = NEXT(lx);
					if (d == '*' && PEEK(lx) == '/') {
						NEXT(lx);
						closed = true;
						break;
					}
				}
				if (!closed) {
					struct source_position p = streamer_position(&lx->s);
					diag_error((struct source_span){p, p}, "unterminated comment");
					skip_to_safe_point(lx);
				}
				continue;
			}
		}
		break;
	}
}

static struct token read_header_name(struct lexer *lx) {
	struct source_position start = streamer_position(&lx->s);
	NEXT(lx);
	vector_of(char) buf = {};
	while (!streamer_eof(&lx->s) && PEEK(lx) != '>' && PEEK(lx) != '\n') {
		vector_push(&buf, NEXT(lx));
	}
	if (PEEK(lx) == '>') {
		NEXT(lx);
	} else {
		struct source_position p = streamer_position(&lx->s);
		diag_error((struct source_span){start, p}, "unterminated header-name");
		skip_to_safe_point(lx);
		return (struct token){.kind = TOKEN_ERROR, .loc = {start, p}, .val.err = "unterminated header-name"};
	}
	vector_push(&buf, '\0');
	const char *name = intern(buf.data);
	vector_destroy(&buf);
	struct token tok = {.loc.start = start};
	tok.kind = TOKEN_HEADER_NAME;
	tok.val.str = name;
	tok.loc.end = streamer_position(&lx->s);
	return tok;
}

static struct token read_quoted_header_name(struct lexer *lx) {
	struct source_position start = streamer_position(&lx->s);
	NEXT(lx);
	vector_of(char) buf = {};
	while (!streamer_eof(&lx->s) && PEEK(lx) != '"' && PEEK(lx) != '\n') {
		int c = NEXT(lx);
		if (c == '\\' && (streamer_peek(&lx->s) == '"' || streamer_peek(&lx->s) == '\\')) {
			c = NEXT(lx);
		}
		vector_push(&buf, (char)c);
	}
	if (PEEK(lx) == '"') {
		NEXT(lx);
	} else {
		struct source_position p = streamer_position(&lx->s);
		diag_error((struct source_span){start, p}, "unterminated quoted header-name");
		skip_to_safe_point(lx);
		return (struct token){.kind = TOKEN_ERROR, .loc = {start, p}, .val.err = "unterminated quoted header-name"};
	}
	vector_push(&buf, '\0');
	const char *name = intern(buf.data);
	vector_destroy(&buf);
	struct token tok = {.loc.start = start};
	tok.kind = TOKEN_HEADER_NAME;
	tok.val.str = name;
	tok.loc.end = streamer_position(&lx->s);
	return tok;
}

static struct token read_ident(struct lexer *lx) {
	vector_of(char) buf = {};
	bool saw_ucn = false;
	bool saw_utf8 = false;
	bool saw_gnu_dollar = false;

	struct source_position start = streamer_position(&lx->s);
	while (!streamer_eof(&lx->s)) {
		skip_line_splice(lx);
		int c = PEEK(lx);
		if (isalpha((unsigned char)c) || c == '_' || (lx->ctx->gnu_extensions && c == '$')) {
			vector_push(&buf, NEXT(lx));
			if (c == '$')
				saw_gnu_dollar = true;
		} else if (isdigit((unsigned char)c)) {
			vector_push(&buf, NEXT(lx));
		} else if (c == '\\') {
			struct streamer_blob bl = streamer_get_blob(&lx->s);
			if (bl.cache[3] == '\n' || (bl.cache[3] == '\r' && bl.cache[4] == '\n')) {
				skip_line_splice(lx);
				continue;
			}
			if (bl.cache[3] == 'u' || bl.cache[3] == 'U') {
				saw_ucn = true;
				uint32_t cp = parse_ucn(lx);
				if (cp <= 0x7F) {
					vector_push(&buf, (char)cp);
				} else if (cp <= 0x7FF) {
					vector_push(&buf, (char)(0xC0 | (cp >> 6)));
					vector_push(&buf, (char)(0x80 | (cp & 0x3F)));
				} else if (cp <= 0xFFFF) {
					vector_push(&buf, (char)(0xE0 | (cp >> 12)));
					vector_push(&buf, (char)(0x80 | ((cp >> 6) & 0x3F)));
					vector_push(&buf, (char)(0x80 | (cp & 0x3F)));
				} else {
					vector_push(&buf, (char)(0xF0 | (cp >> 18)));
					vector_push(&buf, (char)(0x80 | ((cp >> 12) & 0x3F)));
					vector_push(&buf, (char)(0x80 | ((cp >> 6) & 0x3F)));
					vector_push(&buf, (char)(0x80 | (cp & 0x3F)));
				}
			} else
				break;

		} else if ((unsigned char)c >= 0x80) {
			saw_utf8 = true;
			if (!utf8_validate_and_append(&buf, &lx->s)) {
				struct source_position pos = streamer_position(&lx->s);
				diag_error((struct source_span){start, pos}, "invalid UTF-8 in identifier");
				vector_push(&buf, '\0');
				const char *err = intern(buf.data);
				vector_destroy(&buf);
				return (struct token){.kind = TOKEN_ERROR, .loc = {start, pos}, .val.err = err};
			}
		} else {
			break;
		}
	}
	vector_push(&buf, '\0');
	const char *interned = intern(buf.data);
	vector_destroy(&buf);

	struct token tok = {.loc.start = start};
	tok.kind = classify_ident(interned, lx->in_directive);
	tok.val.str = interned;
	tok.loc.end = streamer_position(&lx->s);

	if (saw_ucn && !yecc_std_at_least(lx->ctx, YECC_LANG_C99)) {
		diag_extension(lx, (struct source_span){start, tok.loc.end},
					   "universal-character-name in identifier requires C99 or later");
	}
	if (saw_utf8 && (lx->ctx->pedantic) && !(lx->ctx->gnu_extensions)) {
		diag_extension(lx, (struct source_span){start, tok.loc.end}, "UTF-8 identifier is a non-standard extension");
	}
	if (saw_gnu_dollar && !lx->ctx->gnu_extensions) {
		diag_extension(lx, (struct source_span){start, tok.loc.end}, "identifier contains '$' (GNU extension)");
	}
	if (tok.kind != TOKEN_IDENTIFIER)
		maybe_warn_ident_use(lx, interned, tok.kind, (struct source_span){start, tok.loc.end});
	return tok;
}

static bool valid_int_suffix(const char *s) {
	int u = 0, l = 0;
	for (const char *p = s; *p; p++) {
		if (*p == 'u' || *p == 'U')
			u++;
		else if (*p == 'l' || *p == 'L')
			l++;
		else
			return false;
	}
	return (u <= 1 && l <= 2);
}

static const struct {
	const char *suffix;
	enum token_float_suffix kind;
} float_suffix_table[] = {
	{"f", TOKEN_FSUF_f},		 {"l", TOKEN_FSUF_l},		{"f16", TOKEN_FSUF_f16},   {"f32", TOKEN_FSUF_f32},
	{"f64", TOKEN_FSUF_f64},	 {"f128", TOKEN_FSUF_f128}, {"f32x", TOKEN_FSUF_f32x}, {"f64x", TOKEN_FSUF_f64x},
	{"f128x", TOKEN_FSUF_f128x}, {"df", TOKEN_FSUF_df},		{"dd", TOKEN_FSUF_dd},	   {"dl", TOKEN_FSUF_dl},
};

static enum token_float_suffix classify_float_suffix_lc(const char *lc) {
	if (!lc || !*lc)
		return TOKEN_FSUF_NONE;

	for (size_t i = 0; i < sizeof(float_suffix_table) / sizeof(float_suffix_table[0]); ++i) {
		if (strcmp(lc, float_suffix_table[i].suffix) == 0)
			return float_suffix_table[i].kind;
	}
	return TOKEN_FSUF_NONE;
}

static struct token read_number(struct lexer *lx) {
	vector_of(char) buf = {}, suf = {};
	struct source_position start = streamer_position(&lx->s);

	bool is_float = false;
	bool is_hex_float = false;

	bool used_bin = false;
	bool used_quote_sep = false;
	bool used_underscore_sep = false;

	enum { BASE_DEC = 10, BASE_HEX = 16, BASE_BIN = 2 } base = BASE_DEC;
	bool in_exp = false;
	bool at_seq_start = true;
	bool prev_was_digit = false;
	bool last_was_sep = false;
	bool saw_hex_sig_digit = false;
	bool saw_p = false;
	bool saw_exp_digit = false;
	bool saw_dec_exp_digit = false;

#undef NEXT_IS_DIGIT_FOR_SEQ
#define NEXT_IS_DIGIT_FOR_SEQ()                                                                                        \
	({                                                                                                                 \
		int n = streamer_peek(&lx->s);                                                                                 \
		int _res = 0;                                                                                                  \
		if (in_exp) {                                                                                                  \
			_res = isdigit((unsigned char)n);                                                                          \
		} else if (base == BASE_BIN) {                                                                                 \
			_res = (n == '0' || n == '1');                                                                             \
		} else if (base == BASE_HEX) {                                                                                 \
			_res = isxdigit((unsigned char)n);                                                                         \
		} else {                                                                                                       \
			_res = isdigit((unsigned char)n);                                                                          \
		}                                                                                                              \
		_res;                                                                                                          \
	})

#define SEP_INVALID_HERE()                                                                                             \
	({                                                                                                                 \
		struct source_position p = streamer_position(&lx->s);                                                          \
		diag_error((struct source_span){start, p}, "invalid placement for digit separator");                           \
	})

#define HANDLE_SEP(ch)                                                                                                 \
	({                                                                                                                 \
		if ((ch) == '\'')                                                                                              \
			used_quote_sep = true;                                                                                     \
		if ((ch) == '_')                                                                                               \
			used_underscore_sep = true;                                                                                \
		if (at_seq_start || last_was_sep || !NEXT_IS_DIGIT_FOR_SEQ() || !prev_was_digit) {                             \
			SEP_INVALID_HERE();                                                                                        \
		}                                                                                                              \
		last_was_sep = true;                                                                                           \
	})

#define PUSH_CHAR_RAW(ch)                                                                                              \
	({                                                                                                                 \
		int __c = (ch);                                                                                                \
		vector_push(&buf, (char)__c);                                                                                  \
		at_seq_start = false;                                                                                          \
		if (in_exp)                                                                                                    \
			prev_was_digit = isdigit((unsigned char)__c) != 0;                                                         \
		else if (base == BASE_BIN)                                                                                     \
			prev_was_digit = (__c == '0' || __c == '1');                                                               \
		else if (base == BASE_HEX)                                                                                     \
			prev_was_digit = isxdigit((unsigned char)__c) != 0;                                                        \
		else                                                                                                           \
			prev_was_digit = isdigit((unsigned char)__c) != 0;                                                         \
		last_was_sep = false;                                                                                          \
	})

#define PUSH_DIGIT(ch)                                                                                                 \
	({                                                                                                                 \
		int _c = (ch);                                                                                                 \
		if (_c == '\'' || _c == '_') {                                                                                 \
			HANDLE_SEP(_c);                                                                                            \
		} else {                                                                                                       \
			PUSH_CHAR_RAW(_c);                                                                                         \
		}                                                                                                              \
	})

	if (streamer_peek(&lx->s) == '0') {
		PUSH_DIGIT(NEXT(lx));
		at_seq_start = true;
		prev_was_digit = false;
		last_was_sep = false;

		int c = streamer_peek(&lx->s);
		if (c == 'x' || c == 'X') {
			base = BASE_HEX;
			PUSH_DIGIT(NEXT(lx));
			while (isxdigit((unsigned char)streamer_peek(&lx->s)) || streamer_peek(&lx->s) == '\'' ||
				   streamer_peek(&lx->s) == '_') {
				if (isxdigit((unsigned char)streamer_peek(&lx->s)))
					saw_hex_sig_digit = true;
				PUSH_DIGIT(NEXT(lx));
			}

			if (streamer_peek(&lx->s) == '.' || streamer_peek(&lx->s) == 'p' || streamer_peek(&lx->s) == 'P') {
				is_float = true;
				is_hex_float = true;
				at_seq_start = true;
				prev_was_digit = false;
				last_was_sep = false;

				if (streamer_peek(&lx->s) == '.') {
					PUSH_DIGIT(NEXT(lx));
					while (isxdigit((unsigned char)streamer_peek(&lx->s)) || streamer_peek(&lx->s) == '\'' ||
						   streamer_peek(&lx->s) == '_') {
						if (isxdigit((unsigned char)streamer_peek(&lx->s)))
							saw_hex_sig_digit = true;
						PUSH_DIGIT(NEXT(lx));
					}
				}
				if (streamer_peek(&lx->s) == 'p' || streamer_peek(&lx->s) == 'P') {
					in_exp = true;
					saw_p = true;
					at_seq_start = true;
					prev_was_digit = false;
					last_was_sep = false;

					PUSH_CHAR_RAW(NEXT(lx));
					if (streamer_peek(&lx->s) == '+' || streamer_peek(&lx->s) == '-')
						PUSH_CHAR_RAW(NEXT(lx));
					while (isdigit((unsigned char)streamer_peek(&lx->s)) || streamer_peek(&lx->s) == '\'' ||
						   streamer_peek(&lx->s) == '_') {
						if (isdigit((unsigned char)streamer_peek(&lx->s)))
							saw_exp_digit = true;
						PUSH_DIGIT(NEXT(lx));
					}
				}
			}
		} else if (c == 'b' || c == 'B') {
			used_bin = true;
			base = BASE_BIN;
			PUSH_DIGIT(NEXT(lx));
			at_seq_start = true;
			prev_was_digit = false;
			last_was_sep = false;
			while (streamer_peek(&lx->s) == '0' || streamer_peek(&lx->s) == '1' || streamer_peek(&lx->s) == '\'' ||
				   streamer_peek(&lx->s) == '_')
				PUSH_DIGIT(NEXT(lx));
		} else {
			while (isdigit((unsigned char)streamer_peek(&lx->s)) || streamer_peek(&lx->s) == '\'' ||
				   streamer_peek(&lx->s) == '_')
				PUSH_DIGIT(NEXT(lx));
			if (streamer_peek(&lx->s) == '.') {
				is_float = true;
				at_seq_start = true;
				prev_was_digit = false;
				last_was_sep = false;
				PUSH_DIGIT(NEXT(lx));
				while (isdigit((unsigned char)streamer_peek(&lx->s)) || streamer_peek(&lx->s) == '\'' ||
					   streamer_peek(&lx->s) == '_')
					PUSH_DIGIT(NEXT(lx));
			}
		}
	} else {
		if (streamer_peek(&lx->s) == '.') {
			is_float = true;
			at_seq_start = true;
			prev_was_digit = false;
			last_was_sep = false;
			PUSH_DIGIT(NEXT(lx));
			while (isdigit((unsigned char)streamer_peek(&lx->s)) || streamer_peek(&lx->s) == '\'' ||
				   streamer_peek(&lx->s) == '_')
				PUSH_DIGIT(NEXT(lx));
		} else {
			while (isdigit((unsigned char)streamer_peek(&lx->s)) || streamer_peek(&lx->s) == '\'' ||
				   streamer_peek(&lx->s) == '_')
				PUSH_DIGIT(NEXT(lx));
			if (streamer_peek(&lx->s) == '.') {
				is_float = true;
				at_seq_start = true;
				prev_was_digit = false;
				last_was_sep = false;
				PUSH_DIGIT(NEXT(lx));
				while (isdigit((unsigned char)streamer_peek(&lx->s)) || streamer_peek(&lx->s) == '\'' ||
					   streamer_peek(&lx->s) == '_')
					PUSH_DIGIT(NEXT(lx));
			}
		}
	}

	if (!is_hex_float && (streamer_peek(&lx->s) == 'e' || streamer_peek(&lx->s) == 'E')) {
		is_float = true;
		in_exp = true;
		at_seq_start = true;
		prev_was_digit = false;
		last_was_sep = false;

		PUSH_CHAR_RAW(NEXT(lx));
		if (streamer_peek(&lx->s) == '+' || streamer_peek(&lx->s) == '-')
			PUSH_CHAR_RAW(NEXT(lx));
		while (isdigit((unsigned char)streamer_peek(&lx->s)) || streamer_peek(&lx->s) == '\'' ||
			   streamer_peek(&lx->s) == '_') {
			if (isdigit((unsigned char)streamer_peek(&lx->s)))
				saw_dec_exp_digit = true;
			PUSH_DIGIT(NEXT(lx));
		}
	}

	char low[8] = {};
	if (is_float) {
		char fsuf[8] = {};
		size_t s = 0;
		while (!streamer_eof(&lx->s) && s < sizeof(fsuf) - 1) {
			int ch = streamer_peek(&lx->s);
			if (!isalnum((unsigned char)ch))
				break;
			if (ch == 'i' || ch == 'I' || ch == 'j' || ch == 'J')
				break;
			fsuf[s++] = (char)NEXT(lx);
		}
		fsuf[s] = '\0';
		for (size_t i = 0; i <= s; i++)
			low[i] = (char)tolower((unsigned char)fsuf[i]);

		bool ok_suffix = false;
		if (s == 0) {
			ok_suffix = true;
		} else if (s == 1 && (low[0] == 'f' || low[0] == 'l')) {
			ok_suffix = true;
		} else if (!strcmp(low, "f16") || !strcmp(low, "f32") || !strcmp(low, "f64") || !strcmp(low, "f128") ||
				   !strcmp(low, "f32x") || !strcmp(low, "f64x") || !strcmp(low, "f128x")) {
			ok_suffix = true;
			if (!(lx->ctx->gnu_extensions)) {
				diag_extension(lx, (struct source_span){start, streamer_position(&lx->s)},
							   "floating suffix '%s' requires GNU extensions", fsuf);
			}
		} else if (!strcmp(low, "df") || !strcmp(low, "dd") || !strcmp(low, "dl")) {
			ok_suffix = true;
			if (!(yecc_std_at_least(lx->ctx, YECC_LANG_C23) || lx->ctx->gnu_extensions)) {
				diag_extension(lx, (struct source_span){start, streamer_position(&lx->s)},
							   "decimal floating suffix '%s' requires C23 or GNU extensions", fsuf);
			}
		}
		if (!ok_suffix) {
			diag_error((struct source_span){start, streamer_position(&lx->s)}, "unknown floating suffix '%s'", fsuf);
			return (struct token){.kind = TOKEN_ERROR,
								  .loc = {start, streamer_position(&lx->s)},
								  .val.err = intern("bad floating suffix")};
		}
	} else {
		while (!streamer_eof(&lx->s) && strchr("uUlL", streamer_peek(&lx->s))) {
			char c2 = (char)NEXT(lx);
			vector_push(&suf, c2);
		}
	}

	int ci = streamer_peek(&lx->s);
	struct source_span span_num = {start, streamer_position(&lx->s)};
	if (ci == 'i' || ci == 'I' || ci == 'j' || ci == 'J') {
		(void)NEXT(lx);
		if (yecc_std_at_least(lx->ctx, YECC_LANG_C23)) {
			diag_error(span_num, "imaginary-number suffix is removed in C23");
		} else if (!lx->ctx->gnu_extensions) {
			diag_extension(lx, span_num, "imaginary-number suffix is a non-standard extension");
		}
	}

	vector_push(&buf, '\0');
	vector_push(&suf, '\0');

#undef PUSH_DIGIT
#undef PUSH_CHAR_RAW
#undef HANDLE_SEP
#undef SEP_INVALID_HERE
#undef NEXT_IS_DIGIT_FOR_SEQ

	if (!valid_int_suffix(suf.data)) {
		struct source_position p = streamer_position(&lx->s);
		diag_error((struct source_span){start, p}, "invalid integer suffix '%s'", suf.data);
		return (struct token){.kind = TOKEN_ERROR, .loc = {start, p}, .val.err = intern("bad integer suffix")};
	}

	if (is_hex_float) {
		if (!saw_p) {
			diag_error(span_num, "hexadecimal floating constant requires a 'p' exponent");
			return (struct token){.kind = TOKEN_ERROR, .loc = span_num, .val.err = intern("missing p exponent")};
		} else if (!saw_exp_digit) {
			diag_error(span_num, "exponent has no digits after 'p'");
			return (struct token){.kind = TOKEN_ERROR, .loc = span_num, .val.err = intern("digits after p exponent")};
		}
		if (!saw_hex_sig_digit) {
			diag_error(span_num, "hexadecimal floating constant has no significant hex digits");
			return (struct token){.kind = TOKEN_ERROR, .loc = span_num, .val.err = intern("no significant hex digits")};
		}
	}
	if (is_float && !is_hex_float && in_exp && !saw_dec_exp_digit) {
		diag_error(span_num, "exponent has no digits after 'e'");
		return (struct token){.kind = TOKEN_ERROR, .loc = span_num, .val.err = intern("no digits after e")};
	}

	if (used_bin && !(yecc_std_at_least(lx->ctx, YECC_LANG_C23) || (lx->ctx->gnu_extensions))) {
		diag_extension(lx, span_num, "binary literal '0b...' requires C23 or GNU extensions");
	}
	if (used_quote_sep && !yecc_std_at_least(lx->ctx, YECC_LANG_C23)) {
		diag_extension(lx, span_num, "digit separator '\\'' is not allowed before C23");
	}
	if (used_underscore_sep && !(lx->ctx->gnu_extensions)) {
		diag_extension(lx, span_num, "underscore digit separator is a non-standard extension");
	}
	if (is_hex_float && !(yecc_std_at_least(lx->ctx, YECC_LANG_C99) || lx->ctx->gnu_extensions)) {
		diag_extension(lx, span_num, "hexadecimal floating constant requires C99 or GNU extensions");
	}
	if (is_float && lx->ctx->float_mode == YECC_FLOAT_DISABLED) {
		diag_error(span_num, "floating constants are disabled by configuration");
	}

	struct token tok = {.loc.start = start};
	char *endp = nullptr;

	tok.num_extra.i.base = TOKEN_INT_BASE_NONE;
	tok.num_extra.f.style = TOKEN_FLOAT_DEC;
	tok.num_extra.f.suffix = TOKEN_FSUF_NONE;

	vector_of(char) num = {};
	for (const char *p = buf.data; *p; ++p) {
		if (*p == '\'' || *p == '_')
			continue;
		vector_push(&num, *p);
	}
	vector_push(&num, '\0');

	int is_unsigned = (strchr(suf.data, 'u') != nullptr || strchr(suf.data, 'U') != nullptr);
	int lcount = 0;
	for (const char *p = suf.data; *p; ++p)
		if (*p == 'l' || *p == 'L')
			++lcount;

	if (is_float) {
		errno = 0;
		tok.kind = TOKEN_FLOATING_CONSTANT;
		tok.val.f = strtod(num.data, &endp);
		if (endp != num.data + strlen(num.data)) {
			struct source_position p = streamer_position(&lx->s);
			diag_error((struct source_span){start, p}, "malformed floating constant '%s'", buf.data);
		} else if (errno == ERANGE) {
			if (tok.val.f == 0.0)
				diag_warning(span_num, "floating constant underflow");
			else
				diag_warning(span_num, "floating constant overflow");
		}
		tok.num_extra.f.style = is_hex_float ? TOKEN_FLOAT_HEX : TOKEN_FLOAT_DEC;
		tok.num_extra.f.suffix = classify_float_suffix_lc(low);
	} else {
		errno = 0;
		tok.kind = TOKEN_INTEGER_CONSTANT;

		if (used_bin) {
			if (num.data[2] == '\0') {
				struct source_position p = streamer_position(&lx->s);
				diag_error((struct source_span){start, p}, "malformed binary integer constant '%s'", buf.data);
				vector_destroy(&suf);
				vector_destroy(&buf);
				vector_destroy(&num);
				return (struct token){.kind = TOKEN_ERROR, .loc = {start, p}};
			}

			unsigned long long val = 0;
			bool of = false;
			for (const char *p = num.data + 2; *p; ++p) {
				unsigned bit = (unsigned)(*p - '0');
				if (val > (ULLONG_MAX >> 1) || (val == (ULLONG_MAX >> 1) && bit != 0))
					of = true;
				val = (val << 1) | bit;
			}
			if (of)
				diag_warning(span_num, "integer constant out of range");

			if (is_unsigned) {
				tok.val.u = val;
			} else {
				if (val > (unsigned long long)LLONG_MAX)
					diag_warning(span_num, "integer constant out of range for signed type");
				tok.val.i = (long long)val;
			}
		} else {
			if (num.data[0] == '0' && !(num.data[1] == 'x' || num.data[1] == 'X') &&
				!(num.data[1] == 'b' || num.data[1] == 'B')) {
				for (const char *p = num.data + 1; *p; ++p) {
					if (*p == '8' || *p == '9') {
						diag_error(span_num, "invalid digit '%c' in octal constant", *p);
						break;
					}
				}
			}
			if (is_unsigned)
				tok.val.u = strtoull(num.data, &endp, 0);
			else
				tok.val.i = strtoll(num.data, &endp, 0);

			if (endp != num.data + strlen(num.data)) {
				struct source_position p = streamer_position(&lx->s);
				diag_error((struct source_span){start, p}, "malformed integer constant '%s'", buf.data);
			} else if (errno == ERANGE) {
				diag_warning(span_num, "integer constant out of range");
			}
		}

		if (used_bin)
			tok.num_extra.i.base = TOKEN_INT_BASE_2;
		else if (num.data[0] == '0' && (num.data[1] == 'x' || num.data[1] == 'X'))
			tok.num_extra.i.base = TOKEN_INT_BASE_16;
		else if (num.data[0] == '0' && (vector_size(&num) - 1) != 1)
			tok.num_extra.i.base = TOKEN_INT_BASE_8;
		else
			tok.num_extra.i.base = TOKEN_INT_BASE_10;
	}

	tok.flags = 0;
	if (is_unsigned)
		tok.flags |= TOKEN_FLAG_UNSIGNED;
	if (lcount == 1)
		tok.flags |= TOKEN_FLAG_SIZE_LONG;
	if (lcount >= 2)
		tok.flags |= TOKEN_FLAG_SIZE_LONG_LONG;

	tok.loc.end = streamer_position(&lx->s);
	vector_destroy(&suf);
	vector_destroy(&buf);
	vector_destroy(&num);
	return tok;
}

static uint32_t parse_escape(struct lexer *lx, enum lit_kind lk) {
	int c = NEXT(lx);
	switch (c) {
	case 'a':
		return '\a';
	case 'b':
		return '\b';
	case 'f':
		return '\f';
	case 'n':
		return '\n';
	case 'r':
		return '\r';
	case 't':
		return '\t';
	case 'v':
		return '\v';
	case '\\':
		return '\\';
	case '\'':
		return '\'';
	case '"':
		return '"';
	case '?':
		return '?';

	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7': {
		int code = c - '0', cnt = 1;
		while (cnt < 3) {
			int pk = streamer_peek(&lx->s);
			if (pk < '0' || pk > '7')
				break;
			code = code * 8 + (NEXT(lx) - '0');
			cnt++;
		}
		return (uint32_t)code;
	}

	case 'x': {
		int code = 0, cnt = 0;
		while (isxdigit((unsigned char)streamer_peek(&lx->s))) {
			char d = (char)NEXT(lx);
			code = code * 16 + (isdigit((unsigned char)d) ? d - '0' : toupper((unsigned char)d) - 'A' + 10);
			cnt++;
		}
		if (cnt == 0) {
			struct source_position p = streamer_position(&lx->s);
			diag_error((struct source_span){p, p}, "missing hex digits in escape");
			return 0xFFFD;
		}
		if (lk != LIT_PLAIN) {
			if (code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) {
				struct source_position p = streamer_position(&lx->s);
				diag_warning((struct source_span){p, p},
							 "invalid Unicode scalar value U+%04X in \\x escape; using U+FFFD", (unsigned)code);
				return 0xFFFD;
			}
		}
		return (uint32_t)code;
	}

	case 'u': {
		struct source_position pos = streamer_position(&lx->s);
		uint32_t code = 0;
		for (int i = 0; i < 4; i++) {
			if (!isxdigit((unsigned char)streamer_peek(&lx->s))) {
				struct source_position p = streamer_position(&lx->s);
				diag_error((struct source_span){p, p}, "invalid \\u escape");
				return 0xFFFD;
			}
			char d = (char)NEXT(lx);
			code = (code << 4) + (isdigit((unsigned char)d) ? d - '0' : toupper((unsigned char)d) - 'A' + 10);
		}
		if (code >= 0xD800 && code <= 0xDFFF) {
			diag_error((struct source_span){pos, pos}, "invalid Unicode surrogate");
			return 0xFFFD;
		}
		return code;
	}

	case 'U': {
		struct source_position pos = streamer_position(&lx->s);
		uint32_t code = 0;
		for (int i = 0; i < 8; i++) {
			if (!isxdigit((unsigned char)streamer_peek(&lx->s))) {
				struct source_position p = streamer_position(&lx->s);
				diag_error((struct source_span){p, p}, "invalid \\U escape");
				return 0xFFFD;
			}
			char d = (char)NEXT(lx);
			code = (code << 4) + (isdigit((unsigned char)d) ? d - '0' : toupper((unsigned char)d) - 'A' + 10);
		}
		if (code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) {
			diag_error((struct source_span){pos, pos}, "invalid Unicode code point");
			return 0xFFFD;
		}
		return code;
	}

	default: {
		struct source_position p = streamer_position(&lx->s);
		if (c == 'e') {
			if (lx->ctx->gnu_extensions)
				return 27;
			diag_extension(lx, (struct source_span){p, p}, "\\e is a GNU extension");
			return 27;
		}
		diag_error((struct source_span){p, p}, "unknown escape '\\%c'", c);
		return 0xFFFD;
	}
	}
}

static struct token read_string_literal(struct lexer *lx) {
	struct source_position start = streamer_position(&lx->s);

	enum lit_kind prefix = LIT_PLAIN;
	struct streamer_blob blob = streamer_get_blob(&lx->s);
	if (!streamer_eof(&lx->s)) {
		if (blob.cache[2] == 'u' && blob.cache[3] == '8' && blob.cache[4] == '"') {
			if (!(yecc_std_at_least(lx->ctx, YECC_LANG_C23) || (lx->ctx->gnu_extensions))) {
				struct source_position p = streamer_position(&lx->s);
				diag_extension(lx, (struct source_span){p, p}, "u8 string literal requires C23 or GNU extensions");
			}
			prefix = LIT_UTF8;
			NEXT(lx);
			NEXT(lx);
		} else if (blob.cache[2] == 'u' && blob.cache[3] == '"') {
			prefix = LIT_UTF16;
			NEXT(lx);
		} else if (blob.cache[2] == 'U' && blob.cache[3] == '"') {
			prefix = LIT_UTF32;
			NEXT(lx);
		} else if (blob.cache[2] == 'L' && blob.cache[3] == '"') {
			prefix = LIT_WIDE;
			NEXT(lx);
		} else if ((blob.cache[2] == 'u' || blob.cache[2] == 'U' || blob.cache[2] == 'L') && blob.cache[3] != '"' &&
				   !(blob.cache[2] == 'u' && blob.cache[3] == '8')) {
			struct source_position p = streamer_position(&lx->s);
			diag_error((struct source_span){p, p}, "invalid string literal prefix");
		}
	}

	if (NEXT(lx) != '"') {
		struct source_position p = streamer_position(&lx->s);
		diag_error((struct source_span){start, p}, "internal lexer error: expected '\"'");
		return (struct token){.kind = TOKEN_ERROR, .loc = {start, p}};
	}

	vector_of(uint32_t) cps = {};
	while (!streamer_eof(&lx->s)) {
		int c = NEXT(lx);
		if (c == '"')
			break;

		if (c == '\\') {
			int pk = streamer_peek(&lx->s);
			if (prefix == LIT_PLAIN && (pk == 'u' || pk == 'U')) {
				struct source_position p = streamer_position(&lx->s);
				diag_error((struct source_span){p, p}, "\\u/\\U not allowed in plain string literal");
			}
			uint32_t v = parse_escape(lx, prefix);
			if (prefix == LIT_PLAIN)
				v &= 0xFF;
			vector_push(&cps, v);
			continue;
		}

		if (prefix == LIT_PLAIN) {
			if ((unsigned char)c >= 0x80) {
				struct source_position p = streamer_position(&lx->s);
				diag_error((struct source_span){p, p}, "non-ASCII byte in plain string literal");
				vector_push(&cps, (uint32_t)'?');
			} else {
				vector_push(&cps, (uint8_t)c);
			}
		} else {
			if ((unsigned char)c < 0x80) {
				vector_push(&cps, (uint8_t)c);
			} else {
				streamer_unget(&lx->s);
				uint32_t cp = 0;
				if (!utf8_decode_one(&lx->s, &cp)) {
					cp = 0xFFFD;
				}
				vector_push(&cps, cp);
			}
		}
	}
	if (streamer_eof(&lx->s) && PEEK(lx) != '"') {
		struct source_position p = streamer_position(&lx->s);
		diag_error((struct source_span){start, p}, "unterminated string literal");
		skip_to_safe_point(lx);
	}

	for (;;) {
		skip_space_and_comments(lx);

		struct streamer_blob b2 = streamer_get_blob(&lx->s);
		bool np = (b2.cache[2] == '"');
		bool n8 = (b2.cache[2] == 'u' && b2.cache[3] == '8' && b2.cache[4] == '"');
		bool n16 = (b2.cache[2] == 'u' && b2.cache[3] == '"');
		bool n32 = (b2.cache[2] == 'U' && b2.cache[3] == '"');
		bool nw = (b2.cache[2] == 'L' && b2.cache[3] == '"');

		if (!np && !n8 && !n16 && !n32 && !nw)
			break;

		enum lit_kind next_kind = np ? LIT_PLAIN : n8 ? LIT_UTF8 : n16 ? LIT_UTF16 : n32 ? LIT_UTF32 : LIT_WIDE;
		enum lit_kind promoted = lit_promote(prefix, next_kind, lx->ctx, nullptr);
		if (promoted != prefix) {
			diag_promotion(lx, (struct source_span){start, streamer_position(&lx->s)}, prefix, promoted);
			prefix = promoted;
		}

		if (next_kind == LIT_UTF8) {
			if (!(yecc_std_at_least(lx->ctx, YECC_LANG_C23) || (lx->ctx->gnu_extensions))) {
				struct source_position p = streamer_position(&lx->s);
				diag_extension(lx, (struct source_span){p, p}, "u8 string literal requires C23 or GNU extensions");
			}
			NEXT(lx);
			NEXT(lx);
		} else if (next_kind == LIT_UTF16 || next_kind == LIT_UTF32 || next_kind == LIT_WIDE) {
			NEXT(lx);
		}
		if (NEXT(lx) != '"')
			break;

		while (!streamer_eof(&lx->s)) {
			int c = NEXT(lx);
			if (c == '"')
				break;
			if (c == '\\') {
				int pk = streamer_peek(&lx->s);
				if (prefix == LIT_PLAIN && (pk == 'u' || pk == 'U')) {
					struct source_position p = streamer_position(&lx->s);
					diag_error((struct source_span){p, p}, "\\u/\\U not allowed in plain string literal");
				}
				uint32_t v = parse_escape(lx, prefix);
				if (prefix == LIT_PLAIN)
					v &= 0xFF;
				vector_push(&cps, v);
			} else {
				if (prefix == LIT_PLAIN) {
					if ((unsigned char)c >= 0x80) {
						struct source_position p = streamer_position(&lx->s);
						diag_error((struct source_span){p, p}, "non-ASCII byte in plain string literal");
						vector_push(&cps, (uint32_t)'?');
					} else {
						vector_push(&cps, (uint8_t)c);
					}
				} else {
					if ((unsigned char)c < 0x80) {
						vector_push(&cps, (uint8_t)c);
					} else {
						streamer_unget(&lx->s);
						uint32_t cp = 0;
						if (!utf8_decode_one(&lx->s, &cp))
							cp = 0xFFFD;
						vector_push(&cps, cp);
					}
				}
			}
		}

		if (streamer_eof(&lx->s) && PEEK(lx) != '"') {
			struct source_position p = streamer_position(&lx->s);
			diag_error((struct source_span){start, p}, "unterminated string literal in concatenation");
			skip_to_safe_point(lx);
			break;
		}
	}

	struct token tok = {.loc.start = start};
	tok.kind = TOKEN_STRING_LITERAL;

	switch (prefix) {
	case LIT_WIDE: {
		size_t n = vector_size(&cps);
		wchar_t *wbuf = malloc((n + 1) * sizeof(wchar_t));
		for (size_t i = 0; i < n; i++) {
			uint32_t cp = cps.data[i];
			uint32_t gmax = target_wchar_max(lx->ctx);
			if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
				diag_warning((struct source_span){start, streamer_position(&lx->s)},
							 "invalid Unicode scalar U+%04X in wide string; using U+FFFD", (unsigned)cp);
				cp = 0xFFFD;
			}
			if (cp > gmax) {
				diag_warning((struct source_span){start, streamer_position(&lx->s)},
							 "code point U+%04X not representable in target wchar_t(%ubits); using U+FFFD",
							 (unsigned)cp, lx->ctx ? lx->ctx->wchar_bits : 0);
				cp = 0xFFFD;
			}
			wbuf[i] = (wchar_t)cp;
		}
		wbuf[n] = L'\0';
		tok.val.wstr_lit = wbuf;
		tok.flags = TOKEN_FLAG_STR_WIDE;
	} break;

	case LIT_UTF16: {
		size_t max_units = vector_size(&cps) * 2 + 1;
		char16_t *u16 = malloc(max_units * sizeof(char16_t));
		size_t j = 0;
		vector_foreach(&cps, cp) {
			if (*cp > 0x10FFFF || (*cp >= 0xD800 && *cp <= 0xDFFF)) {
				diag_warning((struct source_span){start, streamer_position(&lx->s)},
							 "invalid Unicode scalar U+%04X in u\"\"; using U+FFFD", (unsigned)*cp);
				*cp = 0xFFFD;
			}
			if (*cp <= 0xFFFF) {
				u16[j++] = (char16_t)*cp;
			} else {
				*cp -= 0x10000;
				u16[j++] = (char16_t)(0xD800 + (*cp >> 10));
				u16[j++] = (char16_t)(0xDC00 + (*cp & 0x3FF));
			}
		}
		u16[j] = 0;
		tok.val.str16_lit = u16;
		tok.flags = TOKEN_FLAG_STR_UTF16;
	} break;

	case LIT_UTF32: {
		size_t n = vector_size(&cps);
		char32_t *u32 = malloc((n + 1) * sizeof(char32_t));
		for (size_t i = 0; i < n; i++) {
			uint32_t cp = cps.data[i];
			if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
				diag_warning((struct source_span){start, streamer_position(&lx->s)},
							 "invalid Unicode scalar U+%04X in U\"\"; using U+FFFD", (unsigned)cp);
				cp = 0xFFFD;
			}
			u32[i] = cp;
		}
		u32[n] = 0;
		tok.val.str32_lit = u32;
		tok.flags = TOKEN_FLAG_STR_UTF32;
	} break;

	case LIT_UTF8:
	case LIT_PLAIN: {
		if (prefix == LIT_UTF8) {
			size_t total = 0;
			vector_foreach(&cps, cp) {
				if (*cp <= 0x7F)
					total += 1;
				else if (*cp <= 0x7FF)
					total += 2;
				else if (*cp <= 0xFFFF)
					total += 3;
				else
					total += 4;
			}
			char8_t *buf = malloc(total + 1);
			size_t pos = 0;
			vector_foreach(&cps, cp) {
				if (*cp > 0x10FFFF || (*cp >= 0xD800 && *cp <= 0xDFFF)) {
					diag_warning((struct source_span){start, streamer_position(&lx->s)},
								 "invalid Unicode scalar U+%04X in u8\"\"; using U+FFFD", (unsigned)*cp);
					*cp = 0xFFFD;
				}
				if (*cp <= 0x7F) {
					buf[pos++] = (char)*cp;
				} else if (*cp <= 0x7FF) {
					buf[pos++] = (char)(0xC0 | (*cp >> 6));
					buf[pos++] = (char)(0x80 | (*cp & 0x3F));
				} else if (*cp <= 0xFFFF) {
					buf[pos++] = (char)(0xE0 | (*cp >> 12));
					buf[pos++] = (char)(0x80 | ((*cp >> 6) & 0x3F));
					buf[pos++] = (char)(0x80 | (*cp & 0x3F));
				} else {
					buf[pos++] = (char)(0xF0 | (*cp >> 18));
					buf[pos++] = (char)(0x80 | ((*cp >> 12) & 0x3F));
					buf[pos++] = (char)(0x80 | ((*cp >> 6) & 0x3F));
					buf[pos++] = (char)(0x80 | (*cp & 0x3F));
				}
			}
			buf[pos] = '\0';
			tok.val.str8_lit = buf;
			tok.flags = TOKEN_FLAG_STR_UTF8;
		} else {
			size_t n = vector_size(&cps);
			char *buf = malloc(n + 1);
			for (size_t i = 0; i < n; i++)
				buf[i] = (char)(cps.data[i] & 0xFF);
			buf[n] = '\0';
			tok.val.str_lit = buf;
			tok.flags = TOKEN_FLAG_STR_PLAIN;
		}
	} break;
	}

	tok.loc.end = streamer_position(&lx->s);
	vector_destroy(&cps);
	return tok;
}

static struct token read_char_literal(struct lexer *lx) {
	struct source_position start = streamer_position(&lx->s);
	bool u8 = false, u16 = false, u32 = false, wide = false;
	struct streamer_blob blob = streamer_get_blob(&lx->s);

	if (blob.cache[2] == 'u' && blob.cache[3] == '\'') {
		u16 = true;
		NEXT(lx);
	} else if (blob.cache[2] == 'u' && blob.cache[3] == '8' && blob.cache[4] == '\'') {
		u8 = true;
		NEXT(lx);
		NEXT(lx);
	} else if (blob.cache[2] == 'U' && blob.cache[3] == '\'') {
		u32 = true;
		NEXT(lx);
	} else if (blob.cache[2] == 'L' && blob.cache[3] == '\'') {
		wide = true;
		NEXT(lx);
	}

	if (NEXT(lx) != '\'') {
		struct source_position p = streamer_position(&lx->s);
		diag_error((struct source_span){start, p}, "internal lexer error: expected '\''");
		return (struct token){.kind = TOKEN_ERROR, .loc = {start, p}};
	}

	vector_of(uint32_t) chars = {};
	while (!streamer_eof(&lx->s)) {
		int c = NEXT(lx);
		if (c == '\'')
			break;
		if (c == '\n')
			goto unterminated_character;

		if (c == '\\') {
			int pk = streamer_peek(&lx->s);
			if (!(u16 || u32 || wide)) {
				if (pk == 'u' || pk == 'U') {
					struct source_position p = streamer_position(&lx->s);
					diag_error((struct source_span){p, p}, "\\u/\\U not allowed in this character literal");
				}
				uint32_t v = parse_escape(lx, LIT_PLAIN);
				if (v == 0xFFFD && (pk == 'x' || pk == 'u' || pk == 'U')) {
					while (!streamer_eof(&lx->s)) {
						int c2 = PEEK(lx);
						if (c2 == '\'') {
							NEXT(lx);
							break;
						}
						if (c2 == '\n')
							break;
						NEXT(lx);
					}
					struct source_position p = streamer_position(&lx->s);
					struct token err = {
						.kind = TOKEN_ERROR,
						.loc = {start, p},
						.val.err = intern("invalid escape in character literal"),
					};
					return err;
				}
				vector_push(&chars, (uint8_t)(v & 0xFF));
				continue;
			}

			enum lit_kind lk = u16 ? LIT_UTF16 : u32 ? LIT_UTF32 : wide ? LIT_WIDE : LIT_UTF8;
			uint32_t v = parse_escape(lx, lk);
			if (v == 0xFFFD && (pk == 'x' || pk == 'u' || pk == 'U')) {
				while (!streamer_eof(&lx->s)) {
					int c2 = PEEK(lx);
					if (c2 == '\'') {
						NEXT(lx);
						break;
					}
					if (c2 == '\n')
						break;
					NEXT(lx);
				}

				struct source_position p = streamer_position(&lx->s);
				struct token err = {
					.kind = TOKEN_ERROR,
					.loc = {start, p},
					.val.err = intern("invalid escape in character literal"),
				};
				return err;
			}
			vector_push(&chars, v);
			continue;
		}

		if (!(u16 || u32 || wide)) {
			if ((unsigned char)c >= 0x80) {
				struct source_position p = streamer_position(&lx->s);
				diag_error((struct source_span){p, p}, "non-ASCII byte in character literal");
				vector_push(&chars, (uint32_t)'?');
			} else {
				vector_push(&chars, (uint8_t)c);
			}
		} else {
			if ((unsigned char)c < 0x80) {
				vector_push(&chars, (uint8_t)c);
			} else {
				streamer_unget(&lx->s);
				uint32_t cp = 0;
				if (!utf8_decode_one(&lx->s, &cp))
					cp = 0xFFFD;
				vector_push(&chars, cp);
			}
		}
	}

	if (streamer_eof(&lx->s)) {
unterminated_character:
		struct source_position p = streamer_position(&lx->s);
		diag_error((struct source_span){start, p}, "unterminated character literal");
		vector_destroy(&chars);
		return (struct token){
			.kind = TOKEN_ERROR, .loc = {start, p}, .val.err = intern("unterminated character literal")};
	}

	if (vector_size(&chars) == 0) {
		struct source_position p = streamer_position(&lx->s);
		diag_error((struct source_span){start, p}, "empty character literal");
		vector_destroy(&chars);
		return (struct token){.kind = TOKEN_ERROR, .loc = {start, p}, .val.err = intern("empty character literal")};
	}

	if (vector_size(&chars) > 1) {
		if (yecc_context_warning_enabled(lx->ctx, YECC_W_MULTICHAR_CHAR)) {
			if (lx->ctx->warnings_as_errors && yecc_context_warning_as_error(lx->ctx, YECC_W_MULTICHAR_CHAR))
				diag_error((struct source_span){start, streamer_position(&lx->s)}, "multi-character character literal");
			else
				diag_warning((struct source_span){start, streamer_position(&lx->s)},
							 "multi-character character literal");
		}
		uint32_t v = 0;
		vector_foreach(&chars, chr) { v = (v << 8) | (*chr & 0xFF); }
		vector_clear(&chars);
		vector_push(&chars, v);
	}

	struct token tok = {.loc.start = start, .kind = TOKEN_CHARACTER_CONSTANT};
	uint32_t cp = chars.data[0];

	if (wide) {
		uint32_t gmax = target_wchar_max(lx->ctx);
		if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
			diag_warning((struct source_span){start, streamer_position(&lx->s)},
						 "invalid Unicode scalar U+%04X in L'…'; using U+FFFD", (unsigned)cp);
			cp = 0xFFFD;
		}
		if (cp > gmax) {
			diag_warning((struct source_span){start, streamer_position(&lx->s)},
						 "code point U+%04X not representable in target wchar_t(%ubits); using U+FFFD", (unsigned)cp,
						 lx->ctx ? lx->ctx->wchar_bits : 0);
			cp = 0xFFFD;
		}
		tok.val.wc = (wchar_t)cp;
		tok.flags = TOKEN_FLAG_STR_WIDE;
	} else if (u16) {
		tok.val.c16 = (char16_t)cp;
		tok.flags = TOKEN_FLAG_STR_UTF16;
	} else if (u32) {
		tok.val.c32 = (char32_t)cp;
		tok.flags = TOKEN_FLAG_STR_UTF32;
	} else if (u8) {
		tok.val.c8 = (char8_t)(cp & 0xFF);
		tok.flags = TOKEN_FLAG_STR_UTF8;
	} else {
		tok.val.c = (char)(cp & 0xFF);
		tok.flags = TOKEN_FLAG_STR_PLAIN;
	}

	tok.loc.end = streamer_position(&lx->s);
	vector_destroy(&chars);
	return tok;
}

static int consume_digraph_token(struct lexer *lx, enum token_kind *out_kind) {
	struct streamer *s = &lx->s;

	char look[4] = {0};
	size_t k = peek_preproc(lx, look, 4);

	const struct alt_entry *hit = nullptr;
	size_t hit_len = 0;
	for (size_t i = 0; i < sizeof(ALT_DI_TABLE) / sizeof(ALT_DI_TABLE[0]); ++i) {
		size_t len = strlen(ALT_DI_TABLE[i].pat);
		if (k >= len && memcmp(look, ALT_DI_TABLE[i].pat, len) == 0) {
			if (len > hit_len) {
				hit = &ALT_DI_TABLE[i];
				hit_len = len;
			}
		}
	}
	if (!hit)
		return 0;

	struct source_position endp = streamer_position(s);
	struct source_position startp = endp;
	if (startp.column >= hit_len)
		startp.column -= hit_len;

	struct source_span sp = {startp, endp};

	if (!lx->ctx->enable_trigraphs) {
		diag_alt_token(lx, sp, "digraph", hit->pat);
		return 0;
	}

	for (size_t j = 0; j < hit_len; ++j)
		(void)NEXT(lx);
	diag_alt_token(lx, sp, "digraph", hit->pat);

	for (size_t i = 0; i < sizeof(puncts) / sizeof(puncts[0]); ++i) {
		if (strcmp(hit->rep, puncts[i].str) == 0) {
			*out_kind = puncts[i].kind;
			return 1;
		}
	}

	diag_error(sp, "internal error: digraph '%s' replacement '%s' not in punct table", hit->pat, hit->rep);
	return -1;
}

static struct token read_punctuator(struct lexer *lx) {
	struct source_position start = streamer_position(&lx->s);

	{
		enum token_kind kdig;
		if (consume_digraph_token(lx, &kdig)) {
			struct token tok = {.loc.start = start, .kind = kdig};
			tok.loc.end = streamer_position(&lx->s);
			return tok;
		}
	}

	for (size_t i = 0; i < sizeof(puncts) / sizeof(puncts[0]); i++) {
		const char *p = puncts[i].str;
		size_t len = strlen(p);
		char tmp[4];
		if (len > sizeof(tmp))
			len = sizeof(tmp);
		size_t k = peek_preproc(lx, tmp, len);
		if (k == len && memcmp(tmp, p, len) == 0) {
			for (size_t j = 0; j < len; j++)
				(void)NEXT(lx);
			struct token tok = {.loc.start = start};
			tok.kind = puncts[i].kind;
			tok.loc.end = streamer_position(&lx->s);
			return tok;
		}
	}

	int bad = NEXT(lx);
	struct source_position p = streamer_position(&lx->s);
	char msg[64];
	snprintf(msg, sizeof(msg), "unexpected character '\\x%02X'", (unsigned char)bad);
	diag_error((struct source_span){start, p}, "%s", msg);
	return (struct token){.loc = {start, p}, .kind = TOKEN_ERROR, .val.err = intern(msg)};
}

bool lexer_init(struct lexer *lx, const char *filename, struct yecc_context *ctx) {
	setlocale(LC_CTYPE, "C");
	setlocale(LC_NUMERIC, "C");

	lx->ctx = ctx;

	if (!streamer_open(&lx->s, filename))
		return false;
	struct streamer_blob blob = streamer_get_blob(&lx->s);
	if (blob.cache[2] == 0xEF && blob.cache[3] == 0xBB && blob.cache[4] == 0xBF) {
		streamer_next(&lx->s);
		streamer_next(&lx->s);
		streamer_next(&lx->s);
		lx->s.column = 0;
	}
	lx->at_line_start = true;
	lx->in_directive = false;
	lx->pp_kind = YECC_PP_NONE;
	lx->expect_header_name = false;
	return true;
}

void lexer_destroy(struct lexer *lx) { streamer_close(&lx->s); }

struct token lexer_next(struct lexer *lx) {
	skip_space_and_comments(lx);

	if (lx->at_line_start) {
		struct source_position saved = streamer_position(&lx->s);
		skip_pp_hspace(lx);

		struct streamer_blob b = streamer_get_blob(&lx->s);
		bool is_hash = (b.cache[2] == '#');
		bool tri_hash = (b.cache[2] == '?' && b.cache[3] == '?' && b.cache[4] == '=');
		bool dig_hash = (b.cache[2] == '%' && b.cache[3] == ':');

		if (is_hash || (lx->ctx->enable_trigraphs && (tri_hash || dig_hash))) {
			lx->at_line_start = false;
			lx->in_directive = true;
			lx->pp_kind = YECC_PP_NONE;
			lx->expect_header_name = false;
			return read_punctuator(lx);
		}

		streamer_seek(&lx->s, saved.offset);
	}

	if (lx->in_directive) {
		skip_pp_hspace(lx);
		if (streamer_peek(&lx->s) == '\n') {
			streamer_next(&lx->s);
			lx->at_line_start = true;
			lx->in_directive = false;
		}
	}

	struct source_position pos = streamer_position(&lx->s);
	if (streamer_eof(&lx->s)) {
		return (struct token){.kind = TOKEN_EOF, .loc = {pos, pos}};
	}

	int c = PEEK(lx);
	struct streamer_blob la = streamer_get_blob(&lx->s);

	if (isdigit((unsigned char)c) || (c == '.' && isdigit((unsigned char)la.cache[3]))) {
		lx->at_line_start = false;
		return read_number(lx);
	}

	if (lx->in_directive && lx->expect_header_name) {
		if ((lx->pp_kind == YECC_PP_INCLUDE || lx->pp_kind == YECC_PP_INCLUDE_NEXT) && c == '<') {
			struct token t = read_header_name(lx);
			lx->expect_header_name = false;
			return t;
		}
		if ((lx->pp_kind == YECC_PP_INCLUDE || lx->pp_kind == YECC_PP_INCLUDE_NEXT || lx->pp_kind == YECC_PP_IMPORT ||
			 lx->pp_kind == YECC_PP_EMBED) &&
			c == '"') {
			struct token t = read_quoted_header_name(lx);
			lx->expect_header_name = false;
			return t;
		}
		lx->expect_header_name = false;
	}

	struct streamer_blob blobie = streamer_get_blob(&lx->s);

	if (c == '"' || ((c == 'u' || c == 'U' || c == 'L') && blobie.cache[3] == '"') ||
		(c == 'u' && blobie.cache[3] == '8' && blobie.cache[4] == '"')) {
		lx->at_line_start = false;
		return read_string_literal(lx);
	}

	if (c == '\'' || ((c == 'u' || c == 'U' || c == 'L') && blobie.cache[3] == '\'') ||
		(c == 'u' && blobie.cache[3] == '8' && blobie.cache[4] == '\'')) {
		lx->at_line_start = false;
		return read_char_literal(lx);
	}

	if (isalpha((unsigned char)c) || c == '_' || (unsigned char)c >= 0x80 || (lx->ctx->gnu_extensions && c == '$')) {
		lx->at_line_start = false;
		return read_ident(lx);
	}

	lx->at_line_start = false;
	return read_punctuator(lx);
}
