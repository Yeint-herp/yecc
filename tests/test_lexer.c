#include "context.h"
#include "lexer.h"
#include "print.h"
#include "token.h"
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uchar.h>
#include <wchar.h>

#define RUN(test)                                                                                                      \
	do {                                                                                                               \
		printf("%-40s", #test);                                                                                        \
		fflush(stdout);                                                                                                \
		test();                                                                                                        \
		puts("OK");                                                                                                    \
	} while (0)

static void failf(const char *file, int line, const char *fmt, ...) {
	fprintf(stderr, "\nASSERT FAILED @ %s:%d\n", file, line);
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	abort();
}

#define ASSERT(expr)                                                                                                   \
	do {                                                                                                               \
		if (!(expr))                                                                                                   \
			failf(__FILE__, __LINE__, "expr: %s", #expr);                                                              \
	} while (0)

#define EPS 1e-12

static char tmpdir_template[] = "/tmp/lexer_test_XXXXXX";
static char *g_tmpdir = nullptr;

static char *make_path(const char *name) {
	size_t n1 = strlen(g_tmpdir), n2 = strlen(name);
	char *p = (char *)malloc(n1 + 1 + n2 + 1);
	memcpy(p, g_tmpdir, n1);
	p[n1] = '/';
	memcpy(p + n1 + 1, name, n2 + 1);
	return p;
}

static void write_file_bytes(const char *name, const uint8_t *data, size_t len) {
	char *path = make_path(name);
	FILE *f = fopen(path, "wb");
	ASSERT(f);
	ASSERT(fwrite(data, 1, len, f) == len);
	fclose(f);
	free(path);
}

static void write_file_str(const char *name, const char *s) { write_file_bytes(name, (const uint8_t *)s, strlen(s)); }

static struct token next_tok(struct lexer *lx) { return lexer_next(lx); }

static struct token next_tok_dbg(struct lexer *lx, const char *ctx) {
	struct token t = lexer_next(lx);
	(void)ctx;
	return t;
}

static unsigned pack_multichar_u8(const char *bytes, size_t n) {
	unsigned v = 0;
	for (size_t i = 0; i < n; ++i) {
		v = (v << 8) | ((unsigned)(unsigned char)bytes[i] & 0xFF);
	}
	return v;
}

#define expect_kind(lx, k) expect_kind_impl(__FILE__, __LINE__, (lx), (k))
#define expect_kind_get(lx, k) expect_kind_get_impl(__FILE__, __LINE__, (lx), (k))
#define expect_ident(lx, name) expect_ident_impl(__FILE__, __LINE__, (lx), (name))
#define expect_keyword(lx, kw, spelling_opt) expect_keyword_impl(__FILE__, __LINE__, (lx), (kw), (spelling_opt))
#define expect_headername(lx, name) expect_headername_impl(__FILE__, __LINE__, (lx), (name))
#define expect_int_s(lx, i, base) expect_int_s_impl(__FILE__, __LINE__, (lx), (i), (base))
#define expect_float(lx, v, style, suf) expect_float_impl(__FILE__, __LINE__, (lx), (v), (style), (suf))

#define expect_str_plain(lx, s) expect_str_plain_impl(__FILE__, __LINE__, (lx), (s))
#define expect_str_u8(lx, s) expect_str_u8_impl(__FILE__, __LINE__, (lx), (s))
#define expect_str_u16(lx, s16) expect_str_u16_impl(__FILE__, __LINE__, (lx), (s16))
#define expect_str_u32(lx, s32) expect_str_u32_impl(__FILE__, __LINE__, (lx), (s32))
#define expect_str_wide(lx, ws) expect_str_wide_impl(__FILE__, __LINE__, (lx), (ws))

#define expect_char_plain(lx, c) expect_char_plain_impl(__FILE__, __LINE__, (lx), (c))
#define expect_char_u8(lx, c8) expect_char_u8_impl(__FILE__, __LINE__, (lx), (c8))
#define expect_char_u16(lx, c16) expect_char_u16_impl(__FILE__, __LINE__, (lx), (c16))
#define expect_char_u32(lx, c32) expect_char_u32_impl(__FILE__, __LINE__, (lx), (c32))
#define expect_char_wide(lx, wc) expect_char_wide_impl(__FILE__, __LINE__, (lx), (wc))

#define expect_char_packed_plain(lx, bytes, n) expect_char_packed_plain_impl(__FILE__, __LINE__, (lx), (bytes), (n))
#define expect_char_packed_u16(lx, bytes, n) expect_char_packed_u16_impl(__FILE__, __LINE__, (lx), (bytes), (n))
#define expect_char_packed_u32(lx, bytes, n) expect_char_packed_u32_impl(__FILE__, __LINE__, (lx), (bytes), (n))
#define expect_char_packed_wide(lx, bytes, n) expect_char_packed_wide_impl(__FILE__, __LINE__, (lx), (bytes), (n))

static void expect_kind_impl(const char *file, int line, struct lexer *lx, enum token_kind k) {
	struct token t = next_tok_dbg(lx, "expect_kind");
	if (t.kind != k) {
		dump_token(&t, "actual");
		failf(file, line, "expected kind: %s (%d), got: %s (%d)", token_kind_name(k), k, token_kind_name(t.kind),
			  t.kind);
	}
}

static struct token expect_kind_get_impl(const char *file, int line, struct lexer *lx, enum token_kind k) {
	struct token t = next_tok_dbg(lx, "expect_kind_get");
	if (t.kind != k) {
		dump_token(&t, "actual");
		failf(file, line, "expected kind: %s (%d), got: %s (%d)", token_kind_name(k), k, token_kind_name(t.kind),
			  t.kind);
	}
	return t;
}

static void expect_ident_impl(const char *file, int line, struct lexer *lx, const char *name) {
	struct token t = expect_kind_get_impl(file, line, lx, TOKEN_IDENTIFIER);
	if (!t.val.str || strcmp(t.val.str, name) != 0) {
		dump_token(&t, "actual");
		failf(file, line, "expected identifier \"%s\", got \"%s\"", name, t.val.str ? t.val.str : "(null)");
	}
}

static void expect_keyword_impl(const char *file, int line, struct lexer *lx, enum token_kind kw,
								const char *spelling_opt) {
	struct token t = expect_kind_get_impl(file, line, lx, kw);
	if (spelling_opt && (!t.val.str || strcmp(t.val.str, spelling_opt) != 0)) {
		dump_token(&t, "actual");
		failf(file, line, "expected keyword spelling \"%s\"", spelling_opt);
	}
}

static void expect_headername_impl(const char *file, int line, struct lexer *lx, const char *name) {
	struct token t = expect_kind_get_impl(file, line, lx, TOKEN_HEADER_NAME);
	if (!t.val.str || strcmp(t.val.str, name) != 0) {
		dump_token(&t, "actual");
		failf(file, line, "expected header-name <%s>, got <%s>", name, t.val.str ? t.val.str : "(null)");
	}
}

static void expect_int_s_impl(const char *file, int line, struct lexer *lx, long long i, int base) {
	struct token t = expect_kind_get_impl(file, line, lx, TOKEN_INTEGER_CONSTANT);
	if ((t.flags & TOKEN_FLAG_UNSIGNED) != 0 || t.val.i != i || t.num_extra.i.base != base) {
		dump_token(&t, "actual");
		failf(file, line, "expected int(signed)=%lld base=%s", i, int_base_name(base));
	}
}

static void expect_float_impl(const char *file, int line, struct lexer *lx, double v, int style, int suf) {
	struct token t = expect_kind_get_impl(file, line, lx, TOKEN_FLOATING_CONSTANT);
	double tol = EPS * fmax(1.0, fabs(v));
	if (fabs(t.val.f - v) > tol || t.num_extra.f.style != style || t.num_extra.f.suffix != suf) {
		dump_token(&t, "actual");
		failf(file, line, "expected float=%g style=%s suf=%s (tol=%g)", v, float_style_name(style), float_suf_name(suf),
			  tol);
	}
}

static void expect_str_plain_impl(const char *file, int line, struct lexer *lx, const char *s) {
	struct token t = expect_kind_get_impl(file, line, lx, TOKEN_STRING_LITERAL);
	if ((t.flags & TOKEN_FLAG_STR_PLAIN) == 0 || !t.val.str_lit || strcmp(t.val.str_lit, s) != 0) {
		dump_token(&t, "actual");
		failf(file, line, "expected plain string \"%s\"", s);
	}
	free(t.val.str_lit);
}

static void expect_str_u8_impl(const char *file, int line, struct lexer *lx, const char *utf8) {
	struct token t = expect_kind_get_impl(file, line, lx, TOKEN_STRING_LITERAL);
	if ((t.flags & TOKEN_FLAG_STR_UTF8) == 0 || !t.val.str_lit || strcmp(t.val.str_lit, utf8) != 0) {
		dump_token(&t, "actual");
		failf(file, line, "expected u8 string \"%s\"", utf8);
	}
	free(t.val.str_lit);
}

static void expect_str_u16_impl(const char *file, int line, struct lexer *lx, const char16_t *u16) {
	struct token t = expect_kind_get_impl(file, line, lx, TOKEN_STRING_LITERAL);
	bool ok = (t.flags & TOKEN_FLAG_STR_UTF16) != 0;
	if (ok) {
		const char16_t *a = u16, *b = t.val.str16_lit;
		while (ok && (*a || *b)) {
			ok = (*a == *b);
			++a;
			++b;
		}
	}
	if (!ok) {
		dump_token(&t, "actual");
		failf(file, line, "expected UTF16 string (exact sequence)");
	}
	free(t.val.str16_lit);
}

static void expect_str_u32_impl(const char *file, int line, struct lexer *lx, const char32_t *u32) {
	struct token t = expect_kind_get_impl(file, line, lx, TOKEN_STRING_LITERAL);
	bool ok = (t.flags & TOKEN_FLAG_STR_UTF32) != 0;
	if (ok) {
		const char32_t *a = u32, *b = t.val.str32_lit;
		while (ok && (*a || *b)) {
			ok = (*a == *b);
			++a;
			++b;
		}
	}
	if (!ok) {
		dump_token(&t, "actual");
		failf(file, line, "expected UTF32 string (exact sequence)");
	}
	free(t.val.str32_lit);
}

static void expect_str_wide_impl(const char *file, int line, struct lexer *lx, const wchar_t *w) {
	struct token t = expect_kind_get_impl(file, line, lx, TOKEN_STRING_LITERAL);
	bool ok = (t.flags & TOKEN_FLAG_STR_WIDE) != 0;
	if (ok) {
		const wchar_t *a = w, *b = t.val.wstr_lit;
		while (ok && (*a || *b)) {
			ok = (*a == *b);
			++a;
			++b;
		}
	}
	if (!ok) {
		dump_token(&t, "actual");
		failf(file, line, "expected wide string (exact sequence)");
	}
	free(t.val.wstr_lit);
}

static void expect_char_plain_impl(const char *file, int line, struct lexer *lx, unsigned char c) {
	struct token t = expect_kind_get_impl(file, line, lx, TOKEN_CHARACTER_CONSTANT);
	if ((t.flags & TOKEN_FLAG_STR_PLAIN) == 0 || (unsigned char)t.val.c != c) {
		dump_token(&t, "actual");
		failf(file, line, "expected plain char 0x%02X, got 0x%02X", (unsigned)c, (unsigned)(unsigned char)t.val.c);
	}
}

static void expect_char_u8_impl(const char *file, int line, struct lexer *lx, char8_t c8) {
	struct token t = expect_kind_get_impl(file, line, lx, TOKEN_CHARACTER_CONSTANT);
	if ((t.flags & TOKEN_FLAG_STR_UTF8) == 0 || t.val.c8 != c8) {
		dump_token(&t, "actual");
		failf(file, line, "expected char8 U+%04X, got U+%04X", (unsigned)c8, (unsigned)t.val.c8);
	}
}

static void expect_char_u16_impl(const char *file, int line, struct lexer *lx, char16_t c) {
	struct token t = expect_kind_get_impl(file, line, lx, TOKEN_CHARACTER_CONSTANT);
	if ((t.flags & TOKEN_FLAG_STR_UTF16) == 0 || t.val.c16 != c) {
		dump_token(&t, "actual");
		failf(file, line, "expected char16 U+%04X, got U+%04X", (unsigned)c, (unsigned)t.val.c16);
	}
}

static void expect_char_u32_impl(const char *file, int line, struct lexer *lx, char32_t c) {
	struct token t = expect_kind_get_impl(file, line, lx, TOKEN_CHARACTER_CONSTANT);
	if ((t.flags & TOKEN_FLAG_STR_UTF32) == 0 || t.val.c32 != c) {
		dump_token(&t, "actual");
		failf(file, line, "expected char32 U+%04X, got U+%04X", (unsigned)c, (unsigned)t.val.c32);
	}
}

static void expect_char_wide_impl(const char *file, int line, struct lexer *lx, wchar_t c) {
	struct token t = expect_kind_get_impl(file, line, lx, TOKEN_CHARACTER_CONSTANT);
	if ((t.flags & TOKEN_FLAG_STR_WIDE) == 0 || t.val.wc != c) {
		dump_token(&t, "actual");
		failf(file, line, "expected wchar U+%04X, got U+%04X", (unsigned)c, (unsigned)t.val.wc);
	}
}

static void expect_char_packed_plain_impl(const char *file, int line, struct lexer *lx, const char *bytes, size_t n) {
	struct token t = expect_kind_get_impl(file, line, lx, TOKEN_CHARACTER_CONSTANT);
	unsigned packed = pack_multichar_u8(bytes, n);
	if ((t.flags & TOKEN_FLAG_STR_PLAIN) == 0) {
		dump_token(&t, "actual");
		failf(file, line, "expected plain multi-char literal, but flags=0x%X", t.flags);
	}
	if ((unsigned)(unsigned char)t.val.c != (packed & 0xFF)) {
		dump_token(&t, "actual");
		failf(file, line, "expected packed plain char 0x%02X, got 0x%02X", (packed & 0xFF),
			  (unsigned)(unsigned char)t.val.c);
	}
}

static void expect_char_packed_u16_impl(const char *file, int line, struct lexer *lx, const char *bytes, size_t n) {
	struct token t = expect_kind_get_impl(file, line, lx, TOKEN_CHARACTER_CONSTANT);
	unsigned packed = pack_multichar_u8(bytes, n);
	if ((t.flags & TOKEN_FLAG_STR_UTF16) == 0) {
		dump_token(&t, "actual");
		failf(file, line, "expected UTF16 multi-char literal, but flags=0x%X", t.flags);
	}
	if ((unsigned)t.val.c16 != (packed & 0xFFFF)) {
		dump_token(&t, "actual");
		failf(file, line, "expected packed UTF16 char 0x%04X, got 0x%04X", (packed & 0xFFFF), (unsigned)t.val.c16);
	}
}

static void expect_char_packed_u32_impl(const char *file, int line, struct lexer *lx, const char *bytes, size_t n) {
	struct token t = expect_kind_get_impl(file, line, lx, TOKEN_CHARACTER_CONSTANT);
	unsigned packed = pack_multichar_u8(bytes, n);
	if ((t.flags & TOKEN_FLAG_STR_UTF32) == 0) {
		dump_token(&t, "actual");
		failf(file, line, "expected UTF32 multi-char literal, but flags=0x%X", t.flags);
	}
	if ((unsigned)t.val.c32 != packed) {
		dump_token(&t, "actual");
		failf(file, line, "expected packed UTF32 char 0x%08X, got 0x%08X", packed, (unsigned)t.val.c32);
	}
}

static void expect_char_packed_wide_impl(const char *file, int line, struct lexer *lx, const char *bytes, size_t n) {
	struct token t = expect_kind_get_impl(file, line, lx, TOKEN_CHARACTER_CONSTANT);
	unsigned packed = pack_multichar_u8(bytes, n);
	if ((t.flags & TOKEN_FLAG_STR_WIDE) == 0) {
		dump_token(&t, "actual");
		failf(file, line, "expected wide multi-char literal, but flags=0x%X", t.flags);
	}
#if WCHAR_MAX == 0xFFFF
	if ((unsigned)t.val.wc != (packed & 0xFFFF)) {
		dump_token(&t, "actual");
		failf(file, line, "expected packed wide char 0x%04X, got 0x%04X", (packed & 0xFFFF), (unsigned)t.val.wc);
	}
#else
	if ((unsigned)t.val.wc != packed) {
		dump_token(&t, "actual");
		failf(file, line, "expected packed wide char 0x%08X, got 0x%08X", packed, (unsigned)t.val.wc);
	}
#endif
}

static void init_ctx(struct yecc_context *ctx, enum yecc_lang_standard std, bool gnu, bool trigraphs, bool pedantic) {
	yecc_context_init(ctx);
	yecc_context_set_lang_standard(ctx, std);
	yecc_context_set_gnu_extensions(ctx, gnu);
	yecc_context_set_enable_trigraphs(ctx, trigraphs);
	yecc_context_set_pedantic(ctx, pedantic);
	yecc_context_set_max_errors(ctx, 50);
}

static void test_bom_and_basic_keywords(void) {
	const uint8_t data[] = {0xEF, 0xBB, 0xBF, 'i', 'n', 't', ' ', 'x', ';', '\n'};
	write_file_bytes("bom.c", data, sizeof(data));

	struct yecc_context ctx;
	init_ctx(&ctx, YECC_LANG_C23, false, false, false);
	struct lexer lx;
	char *p = make_path("bom.c");
	ASSERT(lexer_init(&lx, p, &ctx));

	expect_keyword(&lx, TOKEN_KW_INT, "int");
	expect_ident(&lx, "x");
	expect_kind(&lx, TOKEN_SEMICOLON);
	expect_kind(&lx, TOKEN_EOF);

	lexer_destroy(&lx);
	free(p);
	yecc_context_destroy(&ctx);
}

static void test_directives_and_header_names(void) {
	write_file_str("inc.c", "#   include <stdio.h>\n"
							"#	 include \"my\\\"name.h\"\n"
							"#embed \"res.bin\"\n"
							"#include \"a\\\n.h\""
							"#include <a\\\n.h>");

	struct yecc_context ctx;
	init_ctx(&ctx, YECC_LANG_C23, true, true, true);
	struct lexer lx;
	char *p = make_path("inc.c");
	ASSERT(lexer_init(&lx, p, &ctx));

	expect_kind(&lx, TOKEN_PP_HASH);
	expect_keyword(&lx, TOKEN_PP_INCLUDE, "include");
	expect_headername(&lx, "stdio.h");

	expect_kind(&lx, TOKEN_PP_HASH);
	expect_keyword(&lx, TOKEN_PP_INCLUDE, "include");
	expect_headername(&lx, "my\"name.h");

	expect_kind(&lx, TOKEN_PP_HASH);
	expect_keyword(&lx, TOKEN_PP_EMBED, "embed");
	expect_headername(&lx, "res.bin");

	expect_kind(&lx, TOKEN_PP_HASH);
	expect_keyword(&lx, TOKEN_PP_INCLUDE, "include");
	expect_headername(&lx, "a.h");

	expect_kind(&lx, TOKEN_PP_HASH);
	expect_keyword(&lx, TOKEN_PP_INCLUDE, "include");
	expect_headername(&lx, "a.h");

	expect_kind(&lx, TOKEN_EOF);

	lexer_destroy(&lx);
	free(p);
	yecc_context_destroy(&ctx);
}

static void test_directive_trigraph_digraph_start(void) {
	write_file_str("pptri.c", "??=include <x>\n"
							  "%:include <y>\n");

	{
		struct yecc_context ctx;
		init_ctx(&ctx, YECC_LANG_C23, false, true, false);

		struct lexer lx;
		char *p = make_path("pptri.c");
		ASSERT(lexer_init(&lx, p, &ctx));

		expect_kind(&lx, TOKEN_PP_HASH);
		expect_keyword(&lx, TOKEN_PP_INCLUDE, "include");
		expect_headername(&lx, "x");

		expect_kind(&lx, TOKEN_PP_HASH);
		expect_keyword(&lx, TOKEN_PP_INCLUDE, "include");
		expect_headername(&lx, "y");

		expect_kind(&lx, TOKEN_EOF);

		lexer_destroy(&lx);
		free(p);
		yecc_context_destroy(&ctx);
	}
}

static void test_digraphs_trigraphs_punctuators(void) {
	write_file_str("digtri.c", "<: :> <% %> %: %:%: ??( ??) ??< ??> ??- ??! \"What is going on??!\"\n");

	{
		struct yecc_context ctx;
		init_ctx(&ctx, YECC_LANG_C23, false, true, false);

		struct lexer lx;
		char *p = make_path("digtri.c");
		ASSERT(lexer_init(&lx, p, &ctx));
		expect_kind(&lx, TOKEN_LBRACKET);	 // <:
		expect_kind(&lx, TOKEN_RBRACKET);	 // :>
		expect_kind(&lx, TOKEN_LBRACE);		 // <%
		expect_kind(&lx, TOKEN_RBRACE);		 // %>
		expect_kind(&lx, TOKEN_PP_HASH);	 // %:
		expect_kind(&lx, TOKEN_PP_HASHHASH); // %:%:
		expect_kind(&lx, TOKEN_LBRACKET);	 // ??( -> [
		expect_kind(&lx, TOKEN_RBRACKET);	 // ??)
		expect_kind(&lx, TOKEN_LBRACE);		 // ??<
		expect_kind(&lx, TOKEN_RBRACE);		 // ??>
		expect_kind(&lx, TOKEN_TILDE);		 // ??- -> ~
		expect_kind(&lx, TOKEN_PIPE);		 // ??! -> |
		expect_str_plain(&lx, "What is going on|");

		expect_kind(&lx, TOKEN_EOF);
		lexer_destroy(&lx);
		free(p);
		yecc_context_destroy(&ctx);
	}

	{
		struct yecc_context ctx;
		init_ctx(&ctx, YECC_LANG_C23, false, false, false);

		struct lexer lx;
		char *p = make_path("digtri.c");
		ASSERT(lexer_init(&lx, p, &ctx));

		expect_kind(&lx, TOKEN_LT);		 // <
		expect_kind(&lx, TOKEN_COLON);	 // :
		expect_kind(&lx, TOKEN_COLON);	 // :
		expect_kind(&lx, TOKEN_GT);		 // >
		expect_kind(&lx, TOKEN_LT);		 // <
		expect_kind(&lx, TOKEN_PERCENT); // %
		expect_kind(&lx, TOKEN_PERCENT); // %
		expect_kind(&lx, TOKEN_GT);		 // >
		expect_kind(&lx, TOKEN_PERCENT); // %
		expect_kind(&lx, TOKEN_COLON);	 // :
		expect_kind(&lx, TOKEN_PERCENT); // %
		expect_kind(&lx, TOKEN_COLON);	 // :
		expect_kind(&lx, TOKEN_PERCENT); // %
		expect_kind(&lx, TOKEN_COLON);	 // :

		expect_kind(&lx, TOKEN_QUESTION);	 // ?
		expect_kind(&lx, TOKEN_QUESTION);	 // ?
		expect_kind(&lx, TOKEN_LPAREN);		 // (
		expect_kind(&lx, TOKEN_QUESTION);	 // ?
		expect_kind(&lx, TOKEN_QUESTION);	 // ?
		expect_kind(&lx, TOKEN_RPAREN);		 // )
		expect_kind(&lx, TOKEN_QUESTION);	 // ?
		expect_kind(&lx, TOKEN_QUESTION);	 // ?
		expect_kind(&lx, TOKEN_LT);			 // <
		expect_kind(&lx, TOKEN_QUESTION);	 // ?
		expect_kind(&lx, TOKEN_QUESTION);	 // ?
		expect_kind(&lx, TOKEN_GT);			 // >
		expect_kind(&lx, TOKEN_QUESTION);	 // ?
		expect_kind(&lx, TOKEN_QUESTION);	 // ?
		expect_kind(&lx, TOKEN_MINUS);		 // -
		expect_kind(&lx, TOKEN_QUESTION);	 // ?
		expect_kind(&lx, TOKEN_QUESTION);	 // ?
		expect_kind(&lx, TOKEN_EXCLAMATION); // !

		expect_str_plain(&lx, "What is going on??!");

		expect_kind(&lx, TOKEN_EOF);
		lexer_destroy(&lx);
		free(p);
		yecc_context_destroy(&ctx);
	}
}

static void test_whitespace_comments_splices_and_recovery(void) {
	write_file_str("ws.c", "ab\\\ncd  // c99 line comment\n"
						   "/* unterminated\n"
						   "x = 1; /* unterminated goes to ; x consumed */ y; /*/ still a comment starter */\n"
						   "/* ok */ z;\n");

	struct yecc_context ctx;
	init_ctx(&ctx, YECC_LANG_C23, false, false, true);
	struct lexer lx;
	char *p = make_path("ws.c");
	ASSERT(lexer_init(&lx, p, &ctx));

	expect_ident(&lx, "abcd");

	expect_ident(&lx, "y");
	expect_kind(&lx, TOKEN_SEMICOLON);

	expect_ident(&lx, "z");
	expect_kind(&lx, TOKEN_SEMICOLON);

	expect_kind(&lx, TOKEN_EOF);

	lexer_destroy(&lx);
	free(p);
	yecc_context_destroy(&ctx);
}

static void test_identifiers_ucn_utf8_gnu_dollar_and_invalid_utf8(void) {
	uint8_t src[] = {'a',  'b',	 'c',  ' ', '$', 'g', 'n', 'u', ' ',
					 'u',  '\\', 'u',  '0', '0', 'E', '1', ' ', // "u\u00E1"
					 0xE1, 0xBA, 0xBD, ' ',						// (U+1EBF)
					 0xC0, 'A',	 ' ',							// invalid start byte sequence, then A
					 0};
	write_file_bytes("id.c", src, strlen((char *)src));

	// GNU mode so $ is allowed; C89 so UCN warns (but still forms identifier).
	struct yecc_context ctx;
	init_ctx(&ctx, YECC_LANG_C89, true, false, true);
	struct lexer lx;
	char *p = make_path("id.c");
	ASSERT(lexer_init(&lx, p, &ctx));

	expect_ident(&lx, "abc");
	expect_ident(&lx, "$gnu");

	// u\u00E1 => UTF-8 "uá"
	{
		struct token t = expect_kind_get(&lx, TOKEN_IDENTIFIER);
		ASSERT(strcmp(t.val.str, "u\xc3\xa1") == 0);
	}

	// UTF-8 identifier: "ế"
	{
		struct token t = expect_kind_get(&lx, TOKEN_IDENTIFIER);
		ASSERT(strcmp(t.val.str, "\xE1\xBA\xBD") == 0);
	}

	// invalid utf-8 -> TOKEN_ERROR
	{
		struct token t = next_tok(&lx);
		ASSERT(t.kind == TOKEN_ERROR);
		expect_ident(&lx, "A");
	}

	expect_kind(&lx, TOKEN_EOF);

	lexer_destroy(&lx);
	free(p);
	yecc_context_destroy(&ctx);
}

static void test_keywords_spelling_and_modes(void) {
	write_file_str("kw.c", "_Alignas alignas _Static_assert static_assert _Thread_local thread_local\n"
						   "_Bool bool _Imaginary _Noreturn register typeof __attribute__ asm\n");

	struct yecc_context ctx;
	init_ctx(&ctx, YECC_LANG_C23, false, false, true);
	struct lexer lx;
	char *p = make_path("kw.c");
	ASSERT(lexer_init(&lx, p, &ctx));

	expect_keyword(&lx, TOKEN_KW_ALIGNAS, "_Alignas");
	expect_keyword(&lx, TOKEN_KW_ALIGNAS, "alignas");
	expect_keyword(&lx, TOKEN_KW__STATIC_ASSERT, "_Static_assert");
	expect_keyword(&lx, TOKEN_KW__STATIC_ASSERT, "static_assert");
	expect_keyword(&lx, TOKEN_KW_THREAD_LOCAL, "_Thread_local");
	expect_keyword(&lx, TOKEN_KW_THREAD_LOCAL, "thread_local");
	expect_keyword(&lx, TOKEN_KW__BOOL, "_Bool");
	expect_keyword(&lx, TOKEN_KW_BOOL, "bool");
	expect_keyword(&lx, TOKEN_KW_IMAGINARY, "_Imaginary");
	expect_keyword(&lx, TOKEN_KW_NORETURN, "_Noreturn");
	expect_keyword(&lx, TOKEN_KW_REGISTER, "register");

	expect_keyword(&lx, TOKEN_KW_TYPEOF, "typeof");
	expect_keyword(&lx, TOKEN_KW___ATTRIBUTE__, "__attribute__");
	expect_keyword(&lx, TOKEN_KW_ASM, "asm");

	expect_kind(&lx, TOKEN_EOF);
	lexer_destroy(&lx);
	free(p);
	yecc_context_destroy(&ctx);
}

static void test_integers_bases_separators_suffixes_and_errors(void) {
	write_file_str("nums.c", "0 7 0123 0xFF 0b1011 1'234'567 1_2_3 42u 42UL 42ull 999LUU 0b\n");
	struct yecc_context ctx;
	init_ctx(&ctx, YECC_LANG_C23, true, false, true);
	struct lexer lx;
	char *p = make_path("nums.c");
	ASSERT(lexer_init(&lx, p, &ctx));

	expect_int_s(&lx, 0, TOKEN_INT_BASE_10);
	expect_int_s(&lx, 7, TOKEN_INT_BASE_10);
	expect_int_s(&lx, 83, TOKEN_INT_BASE_8);
	expect_int_s(&lx, 0xFF, TOKEN_INT_BASE_16);
	expect_int_s(&lx, 0b1011, TOKEN_INT_BASE_2);

	expect_int_s(&lx, 1234567, TOKEN_INT_BASE_10);
	expect_int_s(&lx, 123, TOKEN_INT_BASE_10);

	{
		struct token t = expect_kind_get(&lx, TOKEN_INTEGER_CONSTANT);
		ASSERT((t.flags & TOKEN_FLAG_UNSIGNED) != 0);
		ASSERT(t.val.u == 42);
	}
	{
		struct token t = expect_kind_get(&lx, TOKEN_INTEGER_CONSTANT);
		ASSERT((t.flags & TOKEN_FLAG_UNSIGNED) != 0);
		ASSERT((t.flags & (TOKEN_FLAG_SIZE_LONG | TOKEN_FLAG_SIZE_LONG_LONG)) == TOKEN_FLAG_SIZE_LONG);
		ASSERT(t.val.u == 42);
	}
	{
		struct token t = expect_kind_get(&lx, TOKEN_INTEGER_CONSTANT);
		ASSERT((t.flags & TOKEN_FLAG_UNSIGNED) != 0);
		ASSERT((t.flags & TOKEN_FLAG_SIZE_LONG_LONG) != 0);
		ASSERT(t.val.u == 42);
	}

	{
		struct token t = next_tok(&lx);
		ASSERT(t.kind == TOKEN_ERROR);
		ASSERT(strstr(t.val.err, "bad integer suffix") != nullptr);
	}

	{
		struct token t = next_tok(&lx);
		ASSERT(t.kind == TOKEN_ERROR);
	}

	expect_kind(&lx, TOKEN_EOF);

	lexer_destroy(&lx);
	free(p);
	yecc_context_destroy(&ctx);
}

static void test_floats_styles_suffixes_and_exponent_rules(void) {
	write_file_str("flt.c", "1.0 1e10 1e+10 1e-10 .5 0x1.fp3 0xAp1 1.0f 1.0l 1.0f32 1.0df 1e+ 0x1.p\n");
	struct yecc_context ctx;
	init_ctx(&ctx, YECC_LANG_C23, true, false, false);
	struct lexer lx;
	char *p = make_path("flt.c");
	ASSERT(lexer_init(&lx, p, &ctx));

	expect_float(&lx, 1.0, TOKEN_FLOAT_DEC, TOKEN_FSUF_NONE);
	expect_float(&lx, 1e10, TOKEN_FLOAT_DEC, TOKEN_FSUF_NONE);
	expect_float(&lx, 1e10, TOKEN_FLOAT_DEC, TOKEN_FSUF_NONE);
	expect_float(&lx, 1e-10, TOKEN_FLOAT_DEC, TOKEN_FSUF_NONE);
	expect_float(&lx, 0.5, TOKEN_FLOAT_DEC, TOKEN_FSUF_NONE);
	expect_float(&lx, ldexp(0x1.fp0, 3), TOKEN_FLOAT_HEX, TOKEN_FSUF_NONE); // 0x1.fp3 == (1 + 15/16)*2^3
	expect_float(&lx, ldexp(0xA, 1), TOKEN_FLOAT_HEX, TOKEN_FSUF_NONE);		// 0xA p1 -> 10 * 2^1 = 20

	expect_float(&lx, 1.0, TOKEN_FLOAT_DEC, TOKEN_FSUF_f);
	expect_float(&lx, 1.0, TOKEN_FLOAT_DEC, TOKEN_FSUF_l);
	expect_float(&lx, 1.0, TOKEN_FLOAT_DEC, TOKEN_FSUF_f32);
	expect_float(&lx, 1.0, TOKEN_FLOAT_DEC, TOKEN_FSUF_df);

	expect_kind(&lx, TOKEN_ERROR);
	expect_kind(&lx, TOKEN_ERROR);

	expect_kind(&lx, TOKEN_EOF);
	lexer_destroy(&lx);
	free(p);
	yecc_context_destroy(&ctx);
}

static void test_plain_and_unicode_strings_concat_and_rules(void) {
	write_file_str(
		"str.c", "\"A\\nB\\x41\" \"C\" pad0  u8\"Žlutý\" pad1  u\"\\u03A9\" pad2  U\"\\U0001F4A9\" pad3  L\"ž\" pad4\n"
				 "\"a\"\"b\" pad5  u8\"a\" pad6  u8\"b\" pad7  \"\\u0041\" pad8  \"\xC3\xA9\" pad9\n");

	struct yecc_context ctx;
	init_ctx(&ctx, YECC_LANG_C23, true, false, true);
	struct lexer lx;
	char *p = make_path("str.c");
	ASSERT(lexer_init(&lx, p, &ctx));

	// plain: "A\nB\x41" "C" -> "A\nBAC"
	expect_str_plain(&lx, "A\nBAC");
	expect_ident(&lx, "pad0");

	// u8"Žlutý" (UTF-8)
	expect_str_u8(&lx, "\xC5\xBD"
					   "lut\xC3\xBD");
	expect_ident(&lx, "pad1");

	// u"\u03A9" -> UTF-16 { 0x03A9, 0 }
	{
		char16_t u16[] = {0x03A9, 0};
		expect_str_u16(&lx, u16);
	}
	expect_ident(&lx, "pad2");

	// U"\U0001F4A9" -> UTF-32 { 0x1F4A9, 0 }
	{
		char32_t u32[] = {0x1F4A9, 0};
		expect_str_u32(&lx, u32);
	}
	expect_ident(&lx, "pad3");

	// L"ž"
	{
		wchar_t w[] = {L'ž', 0};
		expect_str_wide(&lx, w);
	}
	expect_ident(&lx, "pad4");

	// concatenation: "a""b" -> "ab"
	expect_str_plain(&lx, "ab");
	expect_ident(&lx, "pad5");

	// u8 concatenation with same prefix
	expect_str_u8(&lx, "a");
	expect_ident(&lx, "pad6");
	expect_str_u8(&lx, "b");
	expect_ident(&lx, "pad7");

	// plain string with \u is diagnosed but still inserts 'A'
	expect_str_plain(&lx, "A");
	expect_ident(&lx, "pad8");

	// plain string with non-ASCII (é) becomes '??' (two bytes)
	expect_str_plain(&lx, "??");
	expect_ident(&lx, "pad9");

	expect_kind(&lx, TOKEN_EOF);
	lexer_destroy(&lx);
	free(p);
	yecc_context_destroy(&ctx);
}

static void test_char_literals_escapes_multichar_and_kinds(void) {
	write_file_str("char.c", "'A' '\\n' '\\x41' '\\141' 'ab' u'ď' U'Ω' L'ž'  '\\u0041'  '\\xC3'\n");
	struct yecc_context ctx;
	init_ctx(&ctx, YECC_LANG_C23, true, false, true);
	struct lexer lx;
	char *p = make_path("char.c");
	ASSERT(lexer_init(&lx, p, &ctx));

	expect_char_plain(&lx, 'A');
	expect_char_plain(&lx, '\n');
	expect_char_plain(&lx, 'A');
	expect_char_plain(&lx, '\141');

	{
		struct token t = expect_kind_get(&lx, TOKEN_CHARACTER_CONSTANT);
		ASSERT((t.flags & TOKEN_FLAG_STR_PLAIN) != 0);
		unsigned int packed = ((unsigned)'a' << 8) | (unsigned)'b';
		ASSERT((unsigned)(unsigned char)t.val.c ==
			   (packed & 0xFF)); // only low byte stored in char val; the lexer collapses and saves to 1 cp
	}

	expect_char_u16(&lx, (char16_t)0x010F); // ď
	expect_char_u32(&lx, (char32_t)L'Ω');
	expect_char_wide(&lx, L'ž');

	// plain char with \u is diagnosed but becomes 'A'
	expect_char_plain(&lx, 'A');

	// '\\xC3' -> 0xC3 masked into plain char
	expect_char_plain(&lx, (unsigned char)0xC3);

	expect_kind(&lx, TOKEN_EOF);
	lexer_destroy(&lx);
	free(p);
	yecc_context_destroy(&ctx);
}

static void test_punctuators_greedy(void) {
	write_file_str("punct.c", ">>= >>= >> >= -> ++ -- && || << >> ... ## %:%: "
							  "+= -= *= /= %= &= ^= |= <= >= == != "
							  "# ? : ; , . + - * / % < > = ! ~ ^ & | ( ) [ ] { }\n");
	struct yecc_context ctx;
	init_ctx(&ctx, YECC_LANG_C23, true, true, false);
	struct lexer lx;
	char *p = make_path("punct.c");
	ASSERT(lexer_init(&lx, p, &ctx));

	expect_kind(&lx, TOKEN_RSHIFT_ASSIGN);
	expect_kind(&lx, TOKEN_RSHIFT_ASSIGN);
	expect_kind(&lx, TOKEN_RSHIFT);
	expect_kind(&lx, TOKEN_GE);
	expect_kind(&lx, TOKEN_ARROW);
	expect_kind(&lx, TOKEN_PLUS_PLUS);
	expect_kind(&lx, TOKEN_MINUS_MINUS);
	expect_kind(&lx, TOKEN_AND_AND);
	expect_kind(&lx, TOKEN_OR_OR);
	expect_kind(&lx, TOKEN_LSHIFT);
	expect_kind(&lx, TOKEN_RSHIFT);
	expect_kind(&lx, TOKEN_ELLIPSIS);
	expect_kind(&lx, TOKEN_PP_HASHHASH);
	expect_kind(&lx, TOKEN_PP_HASHHASH);

	expect_kind(&lx, TOKEN_PLUS_ASSIGN);
	expect_kind(&lx, TOKEN_MINUS_ASSIGN);
	expect_kind(&lx, TOKEN_STAR_ASSIGN);
	expect_kind(&lx, TOKEN_SLASH_ASSIGN);
	expect_kind(&lx, TOKEN_PERCENT_ASSIGN);
	expect_kind(&lx, TOKEN_AMP_ASSIGN);
	expect_kind(&lx, TOKEN_XOR_ASSIGN);
	expect_kind(&lx, TOKEN_OR_ASSIGN);
	expect_kind(&lx, TOKEN_LE);
	expect_kind(&lx, TOKEN_GE);
	expect_kind(&lx, TOKEN_EQEQ);
	expect_kind(&lx, TOKEN_NEQ);

	expect_kind(&lx, TOKEN_PP_HASH);
	expect_kind(&lx, TOKEN_QUESTION);
	expect_kind(&lx, TOKEN_COLON);
	expect_kind(&lx, TOKEN_SEMICOLON);
	expect_kind(&lx, TOKEN_COMMA);
	expect_kind(&lx, TOKEN_PERIOD);
	expect_kind(&lx, TOKEN_PLUS);
	expect_kind(&lx, TOKEN_MINUS);
	expect_kind(&lx, TOKEN_STAR);
	expect_kind(&lx, TOKEN_SLASH);
	expect_kind(&lx, TOKEN_PERCENT);
	expect_kind(&lx, TOKEN_LT);
	expect_kind(&lx, TOKEN_GT);
	expect_kind(&lx, TOKEN_ASSIGN);
	expect_kind(&lx, TOKEN_EXCLAMATION);
	expect_kind(&lx, TOKEN_TILDE);
	expect_kind(&lx, TOKEN_CARET);
	expect_kind(&lx, TOKEN_AMP);
	expect_kind(&lx, TOKEN_PIPE);
	expect_kind(&lx, TOKEN_LPAREN);
	expect_kind(&lx, TOKEN_RPAREN);
	expect_kind(&lx, TOKEN_LBRACKET);
	expect_kind(&lx, TOKEN_RBRACKET);
	expect_kind(&lx, TOKEN_LBRACE);
	expect_kind(&lx, TOKEN_RBRACE);

	expect_kind(&lx, TOKEN_EOF);
	lexer_destroy(&lx);
	free(p);
	yecc_context_destroy(&ctx);
}

static void test_numbers_edgecases_and_imag_suffix(void) {
	write_file_str("nedge.c", "0x 09 0xG 0e0 1e i 1i 2j 3J\n");
	struct yecc_context ctx;
	init_ctx(&ctx, YECC_LANG_C11, false, false, true);
	struct lexer lx;
	char *p = make_path("nedge.c");
	ASSERT(lexer_init(&lx, p, &ctx));

	// "0x", hex integer with no digits -> error token
	(void)next_tok(&lx);

	// "09" octal invalid digit -> still integer token
	(void)next_tok(&lx);

	// "0xG" invalid hex digit -> integer parsed until '0x' (error), next token 'G'
	(void)next_tok(&lx);
	expect_ident(&lx, "G");

	// "0e0" decimal float with exponent 0
	expect_float(&lx, 0.0, TOKEN_FLOAT_DEC, TOKEN_FSUF_NONE);

	// "1e" -> missing exponent digits
	expect_kind(&lx, TOKEN_ERROR);
	expect_ident(&lx, "i");

	// "1i" -> valid imaginary token in current settings
	(void)next_tok(&lx);

	// "2j" and "3J" likewise
	(void)next_tok(&lx);
	(void)next_tok(&lx);

	expect_kind(&lx, TOKEN_EOF);
	lexer_destroy(&lx);
	free(p);
	yecc_context_destroy(&ctx);
}

static void test_line_splice_inside_directive_peeking(void) {
	write_file_str("splicepp.c", "#def\\\n"
								 "ine X 1\n");
	struct yecc_context ctx;
	init_ctx(&ctx, YECC_LANG_C23, false, false, false);
	struct lexer lx;
	char *p = make_path("splicepp.c");
	ASSERT(lexer_init(&lx, p, &ctx));

	expect_kind(&lx, TOKEN_PP_HASH);
	expect_keyword(&lx, TOKEN_PP_DEFINE, "define");
	expect_ident(&lx, "X");
	expect_int_s(&lx, 1, TOKEN_INT_BASE_10);
	expect_kind(&lx, TOKEN_EOF);

	lexer_destroy(&lx);
	free(p);
	yecc_context_destroy(&ctx);
}

static void test_char_literals_full_suite(void) {
	write_file_str("char_full.c", "'\\a' '\\b' '\\f' '\\n' '\\r' '\\t' '\\v' '\\\\' '\\'' '\"' '\\?'\n"
								  "'\\0' '\\7' '\\77' '\\377' '\\400' '\\141' '\\x41' '\\x4A' '\\x0' '\\x'\n"
								  "'\\u0041' u'\\u03A9' U'\\U0001F4A9' L'\\u017E'\n"
								  "'\xC3\xA9' u'ď' U'Ω' L'ž'\n"
								  "'AB' 'ABC' 'A\\x42C' u'AB' u'ABC' U'AB' U'ABC' L'AB' L'ABC'\n");

	struct yecc_context ctx;
	init_ctx(&ctx, YECC_LANG_C23, true, false, true);
	struct lexer lx;
	char *p = make_path("char_full.c");
	ASSERT(lexer_init(&lx, p, &ctx));

	expect_char_plain(&lx, '\a');
	expect_char_plain(&lx, '\b');
	expect_char_plain(&lx, '\f');
	expect_char_plain(&lx, '\n');
	expect_char_plain(&lx, '\r');
	expect_char_plain(&lx, '\t');
	expect_char_plain(&lx, '\v');
	expect_char_plain(&lx, '\\');
	expect_char_plain(&lx, '\'');
	expect_char_plain(&lx, '\"');
	expect_char_plain(&lx, '\?');

	expect_char_plain(&lx, 0);
	expect_char_plain(&lx, 7);
	expect_char_plain(&lx, 077);
	expect_char_plain(&lx, 0377);
	/* \400 == 256 -> low 8 bits in plain => 0x00 */
	expect_char_plain(&lx, (unsigned char)(0400 & 0xFF));
	/* \141 == 'a' */
	expect_char_plain(&lx, '\141');

	expect_char_plain(&lx, 0x41);
	expect_char_plain(&lx, 0x4A);
	expect_char_plain(&lx, 0x00);
	expect_kind(&lx, TOKEN_ERROR);

	/* plain with \u0041 -> diagnosed; masked to 8 bits => 'A' */
	expect_char_plain(&lx, 'A');

	/* u'\u03A9' == 0x03A9 (Ω) */
	expect_char_u16(&lx, (char16_t)0x03A9);
	/* U'\U0001F4A9' == 0x1F4A9 (pile of poo) */
	expect_char_u32(&lx, (char32_t)0x0001F4A9);
	/* L'\u017E' == 'ž' as wide */
	expect_char_wide(&lx, L'ž');

	/* non ascii byte */
	expect_char_plain(&lx, '?');

	/* u'ď' */
	expect_char_u16(&lx, (char16_t)0x010F);
	/* U'Ω' */
	expect_char_u32(&lx, (char32_t)L'Ω');
	/* L'ž' */
	expect_char_wide(&lx, L'ž');

	expect_char_packed_plain(&lx, "AB", 2);
	expect_char_packed_plain(&lx, "ABC", 3);
	{
		struct token t = expect_kind_get(&lx, TOKEN_CHARACTER_CONSTANT);
		ASSERT((t.flags & TOKEN_FLAG_STR_PLAIN) != 0);
		ASSERT((unsigned)(unsigned char)t.val.c == 0x2C);
	}

	expect_char_packed_u16(&lx, "AB", 2);
	expect_char_packed_u16(&lx, "ABC", 3);

	expect_char_packed_u32(&lx, "AB", 2);
	expect_char_packed_u32(&lx, "ABC", 3);

	expect_char_packed_wide(&lx, "AB", 2);
	{
		struct token t = expect_kind_get(&lx, TOKEN_CHARACTER_CONSTANT);
		ASSERT((t.flags & TOKEN_FLAG_STR_WIDE) != 0);
		ASSERT((unsigned)t.val.wc == 0xFFFD);
	}

	expect_kind(&lx, TOKEN_EOF);

	lexer_destroy(&lx);
	free(p);
	yecc_context_destroy(&ctx);
}

static void test_more_integer_suffix_permutations(void) {
	write_file_str("nums_more.c", "1u 1l 1L 1ul 1lu 1uLL 1LLu 1lul 1LLU 1uLl 1ll 1LL 1Ul 1lU\n");
	struct yecc_context ctx;
	init_ctx(&ctx, YECC_LANG_C23, true, false, true);
	struct lexer lx;
	char *p = make_path("nums_more.c");
	ASSERT(lexer_init(&lx, p, &ctx));

	for (int i = 0; i < 14; ++i) {
		struct token t = expect_kind_get(&lx, TOKEN_INTEGER_CONSTANT);
		(void)t;
	}
	expect_kind(&lx, TOKEN_EOF);

	lexer_destroy(&lx);
	free(p);
	yecc_context_destroy(&ctx);
}

static void test_more_float_variants_and_edges(void) {
	write_file_str("flt_more.c", "1. .0 0. 1.F 1.L 1.f 1.l 1.0F 1.0L 1.0f128x 1.0dl "
								 "0x.p1 0x1.p 0x1p+ 0x1p- 0x1p-0 0x.8p4 0x0.8p-2\n");
	struct yecc_context ctx;
	init_ctx(&ctx, YECC_LANG_C23, true, false, false);
	struct lexer lx;
	char *p = make_path("flt_more.c");
	ASSERT(lexer_init(&lx, p, &ctx));

	expect_float(&lx, 1.0, TOKEN_FLOAT_DEC, TOKEN_FSUF_NONE);
	expect_float(&lx, 0.0, TOKEN_FLOAT_DEC, TOKEN_FSUF_NONE);
	expect_float(&lx, 0.0, TOKEN_FLOAT_DEC, TOKEN_FSUF_NONE);

	expect_float(&lx, 1.0, TOKEN_FLOAT_DEC, TOKEN_FSUF_f);
	expect_float(&lx, 1.0, TOKEN_FLOAT_DEC, TOKEN_FSUF_l);
	expect_float(&lx, 1.0, TOKEN_FLOAT_DEC, TOKEN_FSUF_f);
	expect_float(&lx, 1.0, TOKEN_FLOAT_DEC, TOKEN_FSUF_l);
	expect_float(&lx, 1.0, TOKEN_FLOAT_DEC, TOKEN_FSUF_f);
	expect_float(&lx, 1.0, TOKEN_FLOAT_DEC, TOKEN_FSUF_l);
	expect_float(&lx, 1.0, TOKEN_FLOAT_DEC, TOKEN_FSUF_f128x);
	expect_float(&lx, 1.0, TOKEN_FLOAT_DEC, TOKEN_FSUF_dl);

	expect_kind(&lx, TOKEN_ERROR); /* 0x.p1 -> no sig hex digit before '.' */
	expect_kind(&lx, TOKEN_ERROR); /* 0x1.p -> missing exp digits */
	expect_kind(&lx, TOKEN_ERROR); /* 0x1p+ -> missing exp digits */
	expect_kind(&lx, TOKEN_ERROR); /* 0x1p- -> missing exp digits */

	expect_float(&lx, ldexp(1.0, -0), TOKEN_FLOAT_HEX, TOKEN_FSUF_NONE);	/* 0x1p-0 == 1 */
	expect_float(&lx, ldexp(0x10p0, -1), TOKEN_FLOAT_HEX, TOKEN_FSUF_NONE); /* 0x.8p4 == (8/16)*2^4 = 8 */
	expect_float(&lx, 0.125, TOKEN_FLOAT_HEX, TOKEN_FSUF_NONE);				/* 0x0.8p-2 == 1/8 */

	expect_kind(&lx, TOKEN_EOF);

	lexer_destroy(&lx);
	free(p);
	yecc_context_destroy(&ctx);
}

static void test_defined_keyword_only_in_directives(void) {
	write_file_str("defined_kw.c", "defined x\n"
								   "#if defined(X)\n#endif\n");
	struct yecc_context ctx;
	init_ctx(&ctx, YECC_LANG_C23, false, false, false);
	struct lexer lx;
	char *p = make_path("defined_kw.c");
	ASSERT(lexer_init(&lx, p, &ctx));

	expect_ident(&lx, "defined");
	expect_ident(&lx, "x");

	expect_kind(&lx, TOKEN_PP_HASH);
	expect_keyword(&lx, TOKEN_PP_IF, "if");
	expect_keyword(&lx, TOKEN_PP_DEFINED, "defined");
	expect_kind(&lx, TOKEN_LPAREN);
	expect_ident(&lx, "X");
	expect_kind(&lx, TOKEN_RPAREN);
	expect_kind(&lx, TOKEN_PP_HASH);
	expect_keyword(&lx, TOKEN_PP_ENDIF, "endif");
	expect_kind(&lx, TOKEN_EOF);

	lexer_destroy(&lx);
	free(p);
	yecc_context_destroy(&ctx);
}

static void test_mixed_string_promotion_concatenation(void) {
	write_file_str("mixstr.c", "\"A\\nB\\x41\" \"C\"  u8\"\\u017D"
							   "lut"
							   "\\u00FD\"  u\"\\u03A9\"  U\"\\U0001F4A9\"  L\"\\u017E\"\n");

	struct yecc_context ctx;
	init_ctx(&ctx, YECC_LANG_C23, true, false, false);
	yecc_context_warning_enable(&ctx, YECC_W_STRING_WIDTH_PROMOTION, true);
	struct lexer lx;
	char *p = make_path("mixstr.c");
	ASSERT(lexer_init(&lx, p, &ctx));

	const wchar_t exp[] = L"A\nBAC\u017D"
						  "lut"
						  "\u00FD\u03A9\U0001F4A9\u017E";
	expect_str_wide(&lx, exp);

	expect_kind(&lx, TOKEN_EOF);
	lexer_destroy(&lx);
	free(p);
	yecc_context_destroy(&ctx);
}

static void test_plain_string_many_non_ascii_bytes(void) {
	const char raw[] = "\"\xC3\xA9\xE2\x98\x83\""; /* "é☃" in UTF-8 -> 2+3 bytes */
	write_file_bytes("plain_na.c", (const uint8_t *)raw, sizeof(raw) - 1);

	struct yecc_context ctx;
	init_ctx(&ctx, YECC_LANG_C23, true, false, true);
	struct lexer lx;
	char *p = make_path("plain_na.c");
	ASSERT(lexer_init(&lx, p, &ctx));

	expect_str_plain(&lx, "?????"); /* 5 bytes -> 5 '?' */
	expect_kind(&lx, TOKEN_EOF);

	lexer_destroy(&lx);
	free(p);
	yecc_context_destroy(&ctx);
}

static void test_char_literal_edge_errors_and_recovery(void) {
	write_file_str("char_err.c", "'' 'A\n 'A' '\\x' '\\u00' '"
								 "\\\n"
								 "' B 'C'\n");
	struct yecc_context ctx;
	init_ctx(&ctx, YECC_LANG_C23, false, false, true);
	struct lexer lx;
	char *p = make_path("char_err.c");
	ASSERT(lexer_init(&lx, p, &ctx));

	expect_kind(&lx, TOKEN_ERROR); /* empty '' */
	expect_kind(&lx, TOKEN_ERROR); /* unterminated 'A */
	expect_char_plain(&lx, 'A');   /* now a normal one */
	expect_kind(&lx, TOKEN_ERROR); /* bad \x */
	expect_kind(&lx, TOKEN_ERROR); /* bad \u */
	expect_kind(&lx, TOKEN_ERROR); /* splice in char -> empty character literal */

	expect_ident(&lx, "B");
	expect_char_plain(&lx, 'C');
	expect_kind(&lx, TOKEN_EOF);

	lexer_destroy(&lx);
	free(p);
	yecc_context_destroy(&ctx);
}

static void test_headername_unterminated_and_recovery(void) {
	write_file_str("hnerr.c", "#include <stdio.h\n"
							  "int x;\n"
							  "#include \"foo\n"
							  "int y;\n");
	struct yecc_context ctx;
	init_ctx(&ctx, YECC_LANG_C23, false, false, true);
	struct lexer lx;
	char *p = make_path("hnerr.c");
	ASSERT(lexer_init(&lx, p, &ctx));

	expect_kind(&lx, TOKEN_PP_HASH);
	expect_keyword(&lx, TOKEN_PP_INCLUDE, "include");
	expect_kind(&lx, TOKEN_ERROR); /* unterminated <...> */

	/* should recover and lex 'int x;' */
	expect_keyword(&lx, TOKEN_KW_INT, "int");
	expect_ident(&lx, "x");
	expect_kind(&lx, TOKEN_SEMICOLON);

	/* second one: unterminated quoted header-name */
	expect_kind(&lx, TOKEN_PP_HASH);
	expect_keyword(&lx, TOKEN_PP_INCLUDE, "include");
	expect_kind(&lx, TOKEN_ERROR);

	expect_keyword(&lx, TOKEN_KW_INT, "int");
	expect_ident(&lx, "y");
	expect_kind(&lx, TOKEN_SEMICOLON);

	expect_kind(&lx, TOKEN_EOF);

	lexer_destroy(&lx);
	free(p);
	yecc_context_destroy(&ctx);
}

static void test_pp_eof_token_emission(void) {
	write_file_str("ppeof.c", "#define X 1\n"
							  "#pragma once\n");
	struct yecc_context ctx;
	init_ctx(&ctx, YECC_LANG_C23, false, false, false);
	struct lexer lx;
	char *p = make_path("ppeof.c");
	ASSERT(lexer_init(&lx, p, &ctx));

	expect_kind(&lx, TOKEN_PP_HASH);
	expect_keyword(&lx, TOKEN_PP_DEFINE, "define");
	expect_ident(&lx, "X");
	expect_int_s(&lx, 1, TOKEN_INT_BASE_10);

	expect_kind(&lx, TOKEN_PP_HASH);
	expect_keyword(&lx, TOKEN_PP_PRAGMA, "pragma");
	expect_ident(&lx, "once");

	expect_kind(&lx, TOKEN_EOF);
	lexer_destroy(&lx);
	free(p);
	yecc_context_destroy(&ctx);
}

static void test_percent_colon_directive_requires_trigraphs_flag(void) {
	write_file_str("digraph_hash_gate.c", "%:include <x>\n");
	struct yecc_context ctx;
	init_ctx(&ctx, YECC_LANG_C23, false, false, false);
	struct lexer lx;
	char *p = make_path("digraph_hash_gate.c");
	ASSERT(lexer_init(&lx, p, &ctx));

	expect_kind(&lx, TOKEN_PERCENT);
	expect_kind(&lx, TOKEN_COLON);
	expect_ident(&lx, "include");
	expect_kind(&lx, TOKEN_LT);
	expect_ident(&lx, "x");
	expect_kind(&lx, TOKEN_GT);
	expect_kind(&lx, TOKEN_EOF);

	lexer_destroy(&lx);
	free(p);
	yecc_context_destroy(&ctx);
}

static void test_u8_strings_unicode_and_errors(void) {
	write_file_str("u8str1.c", "u8\"\\u00E1\\u03A9\\U0001F4A9\" pad1 "
							   "u8\"Žlutý\" pad2 "
							   "u8\"\\x\" pad3 "
							   "u8\"\\uD800\" pad4 "
							   "u8\"A\\nB\\x41\\t\" \n");

	struct yecc_context ctx;
	init_ctx(&ctx, YECC_LANG_C23, true, false, true);
	struct lexer lx;
	char *p = make_path("u8str1.c");
	ASSERT(lexer_init(&lx, p, &ctx));

	expect_str_u8(&lx, "\xC3\xA1\xCE\xA9\xF0\x9F\x92\xA9");
	expect_ident(&lx, "pad1");

	expect_str_u8(&lx, "\xC5\xBD"
					   "lut\xC3\xBD");
	expect_ident(&lx, "pad2");

	expect_str_u8(&lx, "\xEF\xBF\xBD");
	expect_ident(&lx, "pad3");

	expect_str_u8(&lx, "\xEF\xBF\xBD");
	expect_ident(&lx, "pad4");

	expect_str_u8(&lx, "A\nBA\t");

	expect_kind(&lx, TOKEN_EOF);
	lexer_destroy(&lx);
	free(p);
	yecc_context_destroy(&ctx);
}

static void test_u8_concat_and_line_splice(void) {
	write_file_str("u8concat.c", "u8\"ab\" pad1 "
								 "u8\"xy\" pad2 \n"
								 "u8\"AB\"\n");

	struct yecc_context ctx;
	init_ctx(&ctx, YECC_LANG_C23, true, false, true);
	struct lexer lx;
	char *p = make_path("u8concat.c");
	ASSERT(lexer_init(&lx, p, &ctx));

	expect_str_u8(&lx, "ab");
	expect_ident(&lx, "pad1");

	expect_str_u8(&lx, "xy");
	expect_ident(&lx, "pad2");

	expect_str_u8(&lx, "AB");

	expect_kind(&lx, TOKEN_EOF);
	lexer_destroy(&lx);
	free(p);
	yecc_context_destroy(&ctx);
}

static void test_u8_char_more_cases(void) {
	write_file_str("u8char_more.c", "u8'\\x7A' "
									"u8'\\377' "
									"u8'AB' "
									"u8'\\u0041' "
									"u8'\\x' \n");

	struct yecc_context ctx;
	init_ctx(&ctx, YECC_LANG_C23, true, false, true);
	struct lexer lx;
	char *p = make_path("u8char_more.c");
	ASSERT(lexer_init(&lx, p, &ctx));

	expect_char_u8(&lx, (char8_t)'z');
	expect_char_u8(&lx, (char8_t)0xFF);
	expect_char_u8(&lx, (char8_t)'B');
	expect_char_u8(&lx, (char8_t)'A');

	expect_kind(&lx, TOKEN_ERROR);

	expect_kind(&lx, TOKEN_EOF);
	lexer_destroy(&lx);
	free(p);
	yecc_context_destroy(&ctx);
}

static void test_line_splice_in_identifier_many_times(void) {
	write_file_str("spid.c", "foo\\\nbar\\\n_baz\n");
	struct yecc_context ctx;
	init_ctx(&ctx, YECC_LANG_C23, false, false, true);
	struct lexer lx;
	char *p = make_path("spid.c");
	ASSERT(lexer_init(&lx, p, &ctx));

	expect_ident(&lx, "foobar_baz");
	expect_kind(&lx, TOKEN_EOF);
	lexer_destroy(&lx);
	free(p);
	yecc_context_destroy(&ctx);
}

static void test_comment_edge_patterns(void) {
	write_file_str("cmedge.c", "x/**/y/*/z/*a*/b/***//**/c\n"
							   "/* unterminated...\n"
							   "d; /* closed */ e;\n");
	struct yecc_context ctx;
	init_ctx(&ctx, YECC_LANG_C23, false, false, true);
	struct lexer lx;
	char *p = make_path("cmedge.c");
	ASSERT(lexer_init(&lx, p, &ctx));

	expect_ident(&lx, "x");
	expect_ident(&lx, "y");
	expect_ident(&lx, "b");
	expect_ident(&lx, "c");

	expect_ident(&lx, "e");
	expect_kind(&lx, TOKEN_SEMICOLON);
	expect_kind(&lx, TOKEN_EOF);

	lexer_destroy(&lx);
	free(p);
	yecc_context_destroy(&ctx);
}

static void test_peek_preproc_long_lookahead_boundaries(void) {
	const char *content = "??????????????????????????????????????????????????????"
						  "\\\n"
						  "??????????????????????????????????????????????????????\n";

	write_file_str("peekstres.c", content);
	struct yecc_context ctx;
	init_ctx(&ctx, YECC_LANG_C23, true, true, false);
	struct lexer lx;
	char *p = make_path("peekstres.c");
	ASSERT(lexer_init(&lx, p, &ctx));
	int len = strlen(content);

	for (int i = 0; i < (len - 3); ++i)
		expect_kind(&lx, TOKEN_QUESTION);
	expect_kind(&lx, TOKEN_EOF);

	lexer_destroy(&lx);
	free(p);
	yecc_context_destroy(&ctx);
}

static void test_small_complete_program(void) {
	write_file_str("prog.c", "int main(void){ "
							 "int x = 1 + 2; "
							 "if (x > 0) return x; else return 0; "
							 "}\n");

	struct yecc_context ctx;
	init_ctx(&ctx, YECC_LANG_C23, false, false, false);

	struct lexer lx;
	char *p = make_path("prog.c");
	ASSERT(lexer_init(&lx, p, &ctx));

	expect_keyword(&lx, TOKEN_KW_INT, "int");
	expect_ident(&lx, "main");
	expect_kind(&lx, TOKEN_LPAREN);
	expect_keyword(&lx, TOKEN_KW_VOID, "void");
	expect_kind(&lx, TOKEN_RPAREN);
	expect_kind(&lx, TOKEN_LBRACE);

	expect_keyword(&lx, TOKEN_KW_INT, "int");
	expect_ident(&lx, "x");
	expect_kind(&lx, TOKEN_ASSIGN);
	expect_int_s(&lx, 1, TOKEN_INT_BASE_10);
	expect_kind(&lx, TOKEN_PLUS);
	expect_int_s(&lx, 2, TOKEN_INT_BASE_10);
	expect_kind(&lx, TOKEN_SEMICOLON);

	expect_keyword(&lx, TOKEN_KW_IF, "if");
	expect_kind(&lx, TOKEN_LPAREN);
	expect_ident(&lx, "x");
	expect_kind(&lx, TOKEN_GT);
	expect_int_s(&lx, 0, TOKEN_INT_BASE_10);
	expect_kind(&lx, TOKEN_RPAREN);
	expect_keyword(&lx, TOKEN_KW_RETURN, "return");
	expect_ident(&lx, "x");
	expect_kind(&lx, TOKEN_SEMICOLON);

	expect_keyword(&lx, TOKEN_KW_ELSE, "else");
	expect_keyword(&lx, TOKEN_KW_RETURN, "return");
	expect_int_s(&lx, 0, TOKEN_INT_BASE_10);
	expect_kind(&lx, TOKEN_SEMICOLON);

	expect_kind(&lx, TOKEN_RBRACE);
	expect_kind(&lx, TOKEN_EOF);

	lexer_destroy(&lx);
	free(p);
	yecc_context_destroy(&ctx);
}

static void test_large_program(void) {
	write_file_str("large.c",
				   "#include <stdio.h>\n"
				   "#include \"mylib.h\"\n"
				   "#define MAX(a,b) ((a) > (b) ? (a) : (b))\n"
				   "#define STR(x) #x\n"
				   "#if defined(DEBUG)\n"
				   "#  define LOG(fmt, ...) printf(fmt, __VA_ARGS__)\n"
				   "#else\n"
				   "#  define LOG(fmt, ...) (void)0\n"
				   "#endif\n"
				   "static inline int square(int x) { return x * x; }\n"
				   "typedef struct Point { int x; int y; } Point;\n"
				   "enum Color { RED = 1, GREEN, BLUE };\n"
				   "_Alignas(16) static int data[3] = { 0, 1, 2 };\n"
				   "static const char *title = \"Hello \" \"world\";\n"
				   "static const char8_t *u8msg = u8\"Žlutý kůň\";\n"
				   "static const char16_t u16arr[] = u\"\\u03A9\\u03A9\";\n"
				   "static const char32_t u32arr[] = U\"\\U0001F600\";\n"
				   "static const wchar_t *wmsg = L\"žluťoučký kůň\";\n"
				   "int main(void) {\n"
				   "    int i = 0;\n"
				   "    for (int s = 0; s < 3; ++s) { i += square(s); }\n"
				   "    if (i > 0) { LOG(\"i=%d\\n\", i); } else { LOG(\"zero\\n\"); }\n"
				   "    switch (i) { case 0: i = -1; break; case 1: i = +1; break; default: i = 42; break; }\n"
				   "    return i;\n"
				   "}\n");

	struct yecc_context ctx;
	init_ctx(&ctx, YECC_LANG_C23, true, false, true);

	struct lexer lx;
	char *p = make_path("large.c");
	ASSERT(lexer_init(&lx, p, &ctx));

	expect_kind(&lx, TOKEN_PP_HASH);
	expect_keyword(&lx, TOKEN_PP_INCLUDE, "include");
	expect_headername(&lx, "stdio.h");

	expect_kind(&lx, TOKEN_PP_HASH);
	expect_keyword(&lx, TOKEN_PP_INCLUDE, "include");
	expect_headername(&lx, "mylib.h");

	expect_kind(&lx, TOKEN_PP_HASH);
	expect_keyword(&lx, TOKEN_PP_DEFINE, "define");
	expect_ident(&lx, "MAX");
	expect_kind(&lx, TOKEN_LPAREN);
	expect_ident(&lx, "a");
	expect_kind(&lx, TOKEN_COMMA);
	expect_ident(&lx, "b");
	{
		int saw_body_end = 0;
		while (!saw_body_end) {
			struct token t = next_tok(&lx);
			if (t.kind == TOKEN_PP_HASH) {
				expect_keyword(&lx, TOKEN_PP_DEFINE, "define");
				expect_ident(&lx, "STR");
				expect_kind(&lx, TOKEN_LPAREN);
				expect_ident(&lx, "x");
				expect_kind(&lx, TOKEN_RPAREN);
				expect_kind(&lx, TOKEN_PP_HASH);
				expect_ident(&lx, "x");
				saw_body_end = 1;
			}
			if (t.kind == TOKEN_EOF)
				ASSERT(!"unexpected EOF while scanning macro area");
		}
	}

	expect_kind(&lx, TOKEN_PP_HASH);
	expect_keyword(&lx, TOKEN_PP_IF, "if");
	expect_keyword(&lx, TOKEN_PP_DEFINED, "defined");
	expect_kind(&lx, TOKEN_LPAREN);
	expect_ident(&lx, "DEBUG");
	expect_kind(&lx, TOKEN_RPAREN);

	expect_kind(&lx, TOKEN_PP_HASH);
	expect_keyword(&lx, TOKEN_PP_DEFINE, "define");
	expect_ident(&lx, "LOG");
	expect_kind(&lx, TOKEN_LPAREN);
	expect_ident(&lx, "fmt");
	expect_kind(&lx, TOKEN_COMMA);
	{
		int saw_else = 0;
		while (!saw_else) {
			struct token t = next_tok(&lx);
			if (t.kind == TOKEN_PP_HASH) {
				expect_keyword(&lx, TOKEN_PP_ELSE, "else");
				saw_else = 1;
				break;
			}
			if (t.kind == TOKEN_EOF)
				ASSERT(!"unexpected EOF before #else");
		}
	}

	expect_kind(&lx, TOKEN_PP_HASH);
	expect_keyword(&lx, TOKEN_PP_DEFINE, "define");
	expect_ident(&lx, "LOG");
	{
		int saw_endif = 0;
		while (!saw_endif) {
			struct token t = next_tok(&lx);
			if (t.kind == TOKEN_PP_HASH) {
				expect_keyword(&lx, TOKEN_PP_ENDIF, "endif");
				saw_endif = 1;
				break;
			}
			if (t.kind == TOKEN_EOF)
				ASSERT(!"unexpected EOF before #endif");
		}
	}

	expect_keyword(&lx, TOKEN_KW_STATIC, "static");
	expect_keyword(&lx, TOKEN_KW_INLINE, "inline");
	expect_keyword(&lx, TOKEN_KW_INT, "int");
	expect_ident(&lx, "square");
	expect_kind(&lx, TOKEN_LPAREN);
	expect_keyword(&lx, TOKEN_KW_INT, "int");
	expect_ident(&lx, "x");
	expect_kind(&lx, TOKEN_RPAREN);
	expect_kind(&lx, TOKEN_LBRACE);
	expect_keyword(&lx, TOKEN_KW_RETURN, "return");
	expect_ident(&lx, "x");
	expect_kind(&lx, TOKEN_STAR);
	expect_ident(&lx, "x");
	expect_kind(&lx, TOKEN_SEMICOLON);
	expect_kind(&lx, TOKEN_RBRACE);

	expect_keyword(&lx, TOKEN_KW_TYPEDEF, "typedef");
	expect_keyword(&lx, TOKEN_KW_STRUCT, "struct");
	expect_ident(&lx, "Point");
	expect_kind(&lx, TOKEN_LBRACE);
	expect_keyword(&lx, TOKEN_KW_INT, "int");
	expect_ident(&lx, "x");
	expect_kind(&lx, TOKEN_SEMICOLON);
	expect_keyword(&lx, TOKEN_KW_INT, "int");
	expect_ident(&lx, "y");
	expect_kind(&lx, TOKEN_SEMICOLON);
	expect_kind(&lx, TOKEN_RBRACE);
	expect_ident(&lx, "Point");
	expect_kind(&lx, TOKEN_SEMICOLON);

	expect_keyword(&lx, TOKEN_KW_ENUM, "enum");
	expect_ident(&lx, "Color");
	expect_kind(&lx, TOKEN_LBRACE);
	expect_ident(&lx, "RED");
	expect_kind(&lx, TOKEN_ASSIGN);
	expect_int_s(&lx, 1, TOKEN_INT_BASE_10);
	expect_kind(&lx, TOKEN_COMMA);
	expect_ident(&lx, "GREEN");
	expect_kind(&lx, TOKEN_COMMA);
	expect_ident(&lx, "BLUE");
	expect_kind(&lx, TOKEN_RBRACE);
	expect_kind(&lx, TOKEN_SEMICOLON);

	expect_keyword(&lx, TOKEN_KW_ALIGNAS, "_Alignas");
	expect_kind(&lx, TOKEN_LPAREN);
	expect_int_s(&lx, 16, TOKEN_INT_BASE_10);
	expect_kind(&lx, TOKEN_RPAREN);
	expect_keyword(&lx, TOKEN_KW_STATIC, "static");
	expect_keyword(&lx, TOKEN_KW_INT, "int");
	expect_ident(&lx, "data");
	expect_kind(&lx, TOKEN_LBRACKET);
	expect_int_s(&lx, 3, TOKEN_INT_BASE_10);
	expect_kind(&lx, TOKEN_RBRACKET);
	expect_kind(&lx, TOKEN_ASSIGN);
	expect_kind(&lx, TOKEN_LBRACE);
	expect_int_s(&lx, 0, TOKEN_INT_BASE_10);
	expect_kind(&lx, TOKEN_COMMA);
	expect_int_s(&lx, 1, TOKEN_INT_BASE_10);
	expect_kind(&lx, TOKEN_COMMA);
	expect_int_s(&lx, 2, TOKEN_INT_BASE_10);
	expect_kind(&lx, TOKEN_RBRACE);
	expect_kind(&lx, TOKEN_SEMICOLON);

	expect_keyword(&lx, TOKEN_KW_STATIC, "static");
	expect_keyword(&lx, TOKEN_KW_CONST, "const");
	expect_keyword(&lx, TOKEN_KW_CHAR, "char");
	expect_kind(&lx, TOKEN_STAR);
	expect_ident(&lx, "title");
	expect_kind(&lx, TOKEN_ASSIGN);
	expect_str_plain(&lx, "Hello world");
	expect_kind(&lx, TOKEN_SEMICOLON);

	expect_keyword(&lx, TOKEN_KW_STATIC, "static");
	{
		int found_main = 0;
		struct token prevtok = {0}, t = {0};
		while (!found_main) {
			prevtok = t;
			t = next_tok(&lx);
			if (t.kind == TOKEN_EOF)
				ASSERT(!"unexpected EOF before main");
			if (prevtok.kind == TOKEN_KW_INT && t.kind == TOKEN_IDENTIFIER && t.val.str &&
				strcmp(t.val.str, "main") == 0) {
				found_main = 1;
			}
		}
	}

	expect_kind(&lx, TOKEN_LPAREN);
	expect_keyword(&lx, TOKEN_KW_VOID, "void");
	expect_kind(&lx, TOKEN_RPAREN);
	expect_kind(&lx, TOKEN_LBRACE);

	expect_keyword(&lx, TOKEN_KW_INT, "int");
	expect_ident(&lx, "i");
	expect_kind(&lx, TOKEN_ASSIGN);
	expect_int_s(&lx, 0, TOKEN_INT_BASE_10);
	expect_kind(&lx, TOKEN_SEMICOLON);

	expect_keyword(&lx, TOKEN_KW_FOR, "for");
	expect_kind(&lx, TOKEN_LPAREN);
	expect_keyword(&lx, TOKEN_KW_INT, "int");
	expect_ident(&lx, "s");
	expect_kind(&lx, TOKEN_ASSIGN);
	expect_int_s(&lx, 0, TOKEN_INT_BASE_10);
	expect_kind(&lx, TOKEN_SEMICOLON);
	expect_ident(&lx, "s");
	expect_kind(&lx, TOKEN_LT);
	expect_int_s(&lx, 3, TOKEN_INT_BASE_10);
	expect_kind(&lx, TOKEN_SEMICOLON);
	expect_kind(&lx, TOKEN_PLUS_PLUS);
	expect_ident(&lx, "s");
	expect_kind(&lx, TOKEN_RPAREN);
	expect_kind(&lx, TOKEN_LBRACE);
	expect_ident(&lx, "i");
	expect_kind(&lx, TOKEN_PLUS_ASSIGN);
	expect_ident(&lx, "square");
	expect_kind(&lx, TOKEN_LPAREN);
	expect_ident(&lx, "s");
	expect_kind(&lx, TOKEN_RPAREN);
	expect_kind(&lx, TOKEN_SEMICOLON);
	expect_kind(&lx, TOKEN_RBRACE);

	expect_keyword(&lx, TOKEN_KW_IF, "if");
	expect_kind(&lx, TOKEN_LPAREN);
	expect_ident(&lx, "i");
	expect_kind(&lx, TOKEN_GT);
	expect_int_s(&lx, 0, TOKEN_INT_BASE_10);
	expect_kind(&lx, TOKEN_RPAREN);
	expect_kind(&lx, TOKEN_LBRACE);
	expect_ident(&lx, "LOG");
	expect_kind(&lx, TOKEN_LPAREN);
	expect_str_plain(&lx, "i=%d\n");
	{
		int closed = 0;
		while (!closed) {
			struct token t = next_tok(&lx);
			if (t.kind == TOKEN_SEMICOLON) {
				expect_kind(&lx, TOKEN_RBRACE);
				closed = 1;
				break;
			}
			if (t.kind == TOKEN_EOF)
				ASSERT(!"unexpected EOF in if-body");
		}
	}
	expect_keyword(&lx, TOKEN_KW_ELSE, "else");
	expect_kind(&lx, TOKEN_LBRACE);
	expect_ident(&lx, "LOG");
	expect_kind(&lx, TOKEN_LPAREN);
	expect_str_plain(&lx, "zero\n");
	{
		int closed = 0;
		while (!closed) {
			struct token t = next_tok(&lx);
			if (t.kind == TOKEN_RBRACE) {
				closed = 1;
				break;
			}
			if (t.kind == TOKEN_EOF)
				ASSERT(!"unexpected EOF in else-body");
		}
	}

	expect_keyword(&lx, TOKEN_KW_SWITCH, "switch");
	expect_kind(&lx, TOKEN_LPAREN);
	expect_ident(&lx, "i");
	expect_kind(&lx, TOKEN_RPAREN);
	expect_kind(&lx, TOKEN_LBRACE);

	expect_keyword(&lx, TOKEN_KW_CASE, "case");
	expect_int_s(&lx, 0, TOKEN_INT_BASE_10);
	expect_kind(&lx, TOKEN_COLON);
	expect_ident(&lx, "i");
	expect_kind(&lx, TOKEN_ASSIGN);
	expect_kind(&lx, TOKEN_MINUS);
	expect_int_s(&lx, 1, TOKEN_INT_BASE_10);
	expect_kind(&lx, TOKEN_SEMICOLON);
	expect_keyword(&lx, TOKEN_KW_BREAK, "break");
	expect_kind(&lx, TOKEN_SEMICOLON);

	expect_keyword(&lx, TOKEN_KW_CASE, "case");
	expect_int_s(&lx, 1, TOKEN_INT_BASE_10);
	expect_kind(&lx, TOKEN_COLON);
	expect_ident(&lx, "i");
	expect_kind(&lx, TOKEN_ASSIGN);
	expect_kind(&lx, TOKEN_PLUS);
	expect_int_s(&lx, 1, TOKEN_INT_BASE_10);
	expect_kind(&lx, TOKEN_SEMICOLON);
	expect_keyword(&lx, TOKEN_KW_BREAK, "break");
	expect_kind(&lx, TOKEN_SEMICOLON);

	expect_keyword(&lx, TOKEN_KW_DEFAULT, "default");
	expect_kind(&lx, TOKEN_COLON);
	expect_ident(&lx, "i");
	expect_kind(&lx, TOKEN_ASSIGN);
	expect_int_s(&lx, 42, TOKEN_INT_BASE_10);
	expect_kind(&lx, TOKEN_SEMICOLON);
	expect_keyword(&lx, TOKEN_KW_BREAK, "break");
	expect_kind(&lx, TOKEN_SEMICOLON);

	expect_kind(&lx, TOKEN_RBRACE);

	expect_keyword(&lx, TOKEN_KW_RETURN, "return");
	expect_ident(&lx, "i");
	expect_kind(&lx, TOKEN_SEMICOLON);
	expect_kind(&lx, TOKEN_RBRACE);

	expect_kind(&lx, TOKEN_EOF);

	lexer_destroy(&lx);
	free(p);
	yecc_context_destroy(&ctx);
}

int main(void) {
	setvbuf(stdout, nullptr, _IONBF, 0);
	g_tmpdir = mkdtemp(tmpdir_template);
	ASSERT(g_tmpdir);
	intern_init();
	diag_init(nullptr);

	puts("\n=== LEXER Functional Tests ===");

	RUN(test_bom_and_basic_keywords);
	RUN(test_directives_and_header_names);
	RUN(test_directive_trigraph_digraph_start);
	RUN(test_digraphs_trigraphs_punctuators);
	RUN(test_whitespace_comments_splices_and_recovery);
	RUN(test_identifiers_ucn_utf8_gnu_dollar_and_invalid_utf8);
	RUN(test_keywords_spelling_and_modes);
	RUN(test_integers_bases_separators_suffixes_and_errors);
	RUN(test_floats_styles_suffixes_and_exponent_rules);
	RUN(test_plain_and_unicode_strings_concat_and_rules);
	RUN(test_char_literals_escapes_multichar_and_kinds);
	RUN(test_punctuators_greedy);
	RUN(test_numbers_edgecases_and_imag_suffix);
	RUN(test_line_splice_inside_directive_peeking);
	RUN(test_char_literals_full_suite);
	RUN(test_more_integer_suffix_permutations);
	RUN(test_more_float_variants_and_edges);
	RUN(test_defined_keyword_only_in_directives);
	RUN(test_mixed_string_promotion_concatenation);
	RUN(test_plain_string_many_non_ascii_bytes);
	RUN(test_char_literal_edge_errors_and_recovery);
	RUN(test_headername_unterminated_and_recovery);
	RUN(test_pp_eof_token_emission);
	RUN(test_percent_colon_directive_requires_trigraphs_flag);
	RUN(test_u8_strings_unicode_and_errors);
	RUN(test_u8_concat_and_line_splice);
	RUN(test_u8_char_more_cases);
	RUN(test_line_splice_in_identifier_many_times);
	RUN(test_comment_edge_patterns);
	RUN(test_peek_preproc_long_lookahead_boundaries);
	RUN(test_small_complete_program);
	RUN(test_large_program);

	puts("\nAll tests passed successfully!");

	char cmd[PATH_MAX];
	snprintf(cmd, sizeof(cmd), "rm -rf %s", g_tmpdir);
	system(cmd);
	return 0;
}
