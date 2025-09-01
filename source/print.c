#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <uchar.h>
#include <wchar.h>

#include <print.h>
#include <streamer.h>
#include <token.h>

static void dump_flags_inner(unsigned f, char *out, size_t n);

const char *token_kind_name(enum token_kind k) {
	switch (k) {
	case TOKEN_ERROR:
		return "TOKEN_ERROR";
	case TOKEN_EOF:
		return "TOKEN_EOF";

	case TOKEN_IDENTIFIER:
		return "TOKEN_IDENTIFIER";
	case TOKEN_INTEGER_CONSTANT:
		return "TOKEN_INTEGER_CONSTANT";
	case TOKEN_FLOATING_CONSTANT:
		return "TOKEN_FLOATING_CONSTANT";
	case TOKEN_CHARACTER_CONSTANT:
		return "TOKEN_CHARACTER_CONSTANT";
	case TOKEN_STRING_LITERAL:
		return "TOKEN_STRING_LITERAL";

	case TOKEN_LPAREN:
		return "TOKEN_LPAREN";
	case TOKEN_RPAREN:
		return "TOKEN_RPAREN";
	case TOKEN_LBRACKET:
		return "TOKEN_LBRACKET";
	case TOKEN_RBRACKET:
		return "TOKEN_RBRACKET";
	case TOKEN_LBRACE:
		return "TOKEN_LBRACE";
	case TOKEN_RBRACE:
		return "TOKEN_RBRACE";
	case TOKEN_PERIOD:
		return "TOKEN_PERIOD";
	case TOKEN_ELLIPSIS:
		return "TOKEN_ELLIPSIS";
	case TOKEN_ARROW:
		return "TOKEN_ARROW";

	case TOKEN_PLUS:
		return "TOKEN_PLUS";
	case TOKEN_PLUS_PLUS:
		return "TOKEN_PLUS_PLUS";
	case TOKEN_MINUS:
		return "TOKEN_MINUS";
	case TOKEN_MINUS_MINUS:
		return "TOKEN_MINUS_MINUS";
	case TOKEN_STAR:
		return "TOKEN_STAR";
	case TOKEN_SLASH:
		return "TOKEN_SLASH";
	case TOKEN_PERCENT:
		return "TOKEN_PERCENT";

	case TOKEN_LT:
		return "TOKEN_LT";
	case TOKEN_GT:
		return "TOKEN_GT";
	case TOKEN_LE:
		return "TOKEN_LE";
	case TOKEN_GE:
		return "TOKEN_GE";
	case TOKEN_EQEQ:
		return "TOKEN_EQEQ";
	case TOKEN_NEQ:
		return "TOKEN_NEQ";

	case TOKEN_AMP:
		return "TOKEN_AMP";
	case TOKEN_AND_AND:
		return "TOKEN_AND_AND";
	case TOKEN_PIPE:
		return "TOKEN_PIPE";
	case TOKEN_OR_OR:
		return "TOKEN_OR_OR";
	case TOKEN_CARET:
		return "TOKEN_CARET";
	case TOKEN_TILDE:
		return "TOKEN_TILDE";
	case TOKEN_EXCLAMATION:
		return "TOKEN_EXCLAMATION";

	case TOKEN_QUESTION:
		return "TOKEN_QUESTION";
	case TOKEN_COLON:
		return "TOKEN_COLON";
	case TOKEN_SEMICOLON:
		return "TOKEN_SEMICOLON";
	case TOKEN_COMMA:
		return "TOKEN_COMMA";

	case TOKEN_ASSIGN:
		return "TOKEN_ASSIGN";
	case TOKEN_PLUS_ASSIGN:
		return "TOKEN_PLUS_ASSIGN";
	case TOKEN_MINUS_ASSIGN:
		return "TOKEN_MINUS_ASSIGN";
	case TOKEN_STAR_ASSIGN:
		return "TOKEN_STAR_ASSIGN";
	case TOKEN_SLASH_ASSIGN:
		return "TOKEN_SLASH_ASSIGN";
	case TOKEN_PERCENT_ASSIGN:
		return "TOKEN_PERCENT_ASSIGN";
	case TOKEN_LSHIFT_ASSIGN:
		return "TOKEN_LSHIFT_ASSIGN";
	case TOKEN_RSHIFT_ASSIGN:
		return "TOKEN_RSHIFT_ASSIGN";
	case TOKEN_LSHIFT:
		return "TOKEN_LSHIFT";
	case TOKEN_RSHIFT:
		return "TOKEN_RSHIFT";
	case TOKEN_AMP_ASSIGN:
		return "TOKEN_AMP_ASSIGN";
	case TOKEN_XOR_ASSIGN:
		return "TOKEN_XOR_ASSIGN";
	case TOKEN_OR_ASSIGN:
		return "TOKEN_OR_ASSIGN";

	case TOKEN_PP_HASH:
		return "TOKEN_PP_HASH";
	case TOKEN_PP_HASHHASH:
		return "TOKEN_PP_HASHHASH";
	case TOKEN_PP_DEFINED:
		return "TOKEN_PP_DEFINED";
	case TOKEN_HEADER_NAME:
		return "TOKEN_HEADER_NAME";

	case TOKEN_PP_INCLUDE:
		return "TOKEN_PP_INCLUDE";
	case TOKEN_PP_DEFINE:
		return "TOKEN_PP_DEFINE";
	case TOKEN_PP_UNDEF:
		return "TOKEN_PP_UNDEF";
	case TOKEN_PP_IF:
		return "TOKEN_PP_IF";
	case TOKEN_PP_IFDEF:
		return "TOKEN_PP_IFDEF";
	case TOKEN_PP_IFNDEF:
		return "TOKEN_PP_IFNDEF";
	case TOKEN_PP_ELIF:
		return "TOKEN_PP_ELIF";
	case TOKEN_PP_ELSE:
		return "TOKEN_PP_ELSE";
	case TOKEN_PP_ENDIF:
		return "TOKEN_PP_ENDIF";
	case TOKEN_PP_ERROR:
		return "TOKEN_PP_ERROR";
	case TOKEN_PP_LINE:
		return "TOKEN_PP_LINE";
	case TOKEN_PP_PRAGMA:
		return "TOKEN_PP_PRAGMA";

	case TOKEN_PP_IMPORT:
		return "TOKEN_PP_IMPORT";
	case TOKEN_PP_ELIFDEF:
		return "TOKEN_PP_ELIFDEF";
	case TOKEN_PP_ELIFNDEF:
		return "TOKEN_PP_ELIFNDEF";
	case TOKEN_PP_EMBED:
		return "TOKEN_PP_EMBED";
	case TOKEN_PP_WARNING:
		return "TOKEN_PP_WARNING";
	case TOKEN_PP___HAS_INCLUDE:
		return "TOKEN_PP___HAS_INCLUDE";
	case TOKEN_PP___HAS_C_ATTRIBUTE:
		return "TOKEN_PP___HAS_C_ATTRIBUTE";
	case TOKEN_PP__ASSERT:
		return "TOKEN_PP__ASSERT";
	case TOKEN_PP__ASSERT_ANY:
		return "TOKEN_PP__ASSERT_ANY";
	case TOKEN_PP___VA_OPT__:
		return "TOKEN_PP___VA_OPT__";

	case TOKEN_PP_INCLUDE_NEXT:
		return "TOKEN_PP_INCLUDE_NEXT";
	case TOKEN_PP_IDENT:
		return "TOKEN_PP_IDENT";
	case TOKEN_PP_SCCS:
		return "TOKEN_PP_SCCS";
	case TOKEN_PP_ASSERT:
		return "TOKEN_PP_ASSERT";
	case TOKEN_PP_UNASSERT:
		return "TOKEN_PP_UNASSERT";

	case TOKEN_KW_TYPEOF:
		return "TOKEN_KW_TYPEOF";
	case TOKEN_KW_ASM:
		return "TOKEN_KW_ASM";
	case TOKEN_KW___ASM__:
		return "TOKEN_KW___ASM__";
	case TOKEN_KW___ATTRIBUTE__:
		return "TOKEN_KW___ATTRIBUTE__";
	case TOKEN_KW___BUILTIN_TYPES_COMPATIBLE_P:
		return "TOKEN_KW___BUILTIN_TYPES_COMPATIBLE_P";
	case TOKEN_KW___AUTO_TYPE:
		return "TOKEN_KW___AUTO_TYPE";
	case TOKEN_KW___EXTENSION__:
		return "TOKEN_KW___EXTENSION__";
	case TOKEN_KW___LABEL__:
		return "TOKEN_KW___LABEL__";
	case TOKEN_KW___REAL__:
		return "TOKEN_KW___REAL__";
	case TOKEN_KW___IMAG__:
		return "TOKEN_KW___IMAG__";
	case TOKEN_KW___THREAD:
		return "TOKEN_KW___THREAD";
	case TOKEN_KW___FUNCTION__:
		return "TOKEN_KW___FUNCTION__";

	case TOKEN_KW_TYPEDEF:
		return "TOKEN_KW_TYPEDEF";
	case TOKEN_KW_ALIGNAS:
		return "TOKEN_KW_ALIGNAS";
	case TOKEN_KW_ALIGNOF:
		return "TOKEN_KW_ALIGNOF";
	case TOKEN_KW_ATOMIC:
		return "TOKEN_KW_ATOMIC";
	case TOKEN_KW_AUTO:
		return "TOKEN_KW_AUTO";
	case TOKEN_KW_BOOL:
		return "TOKEN_KW_BOOL";
	case TOKEN_KW_BREAK:
		return "TOKEN_KW_BREAK";
	case TOKEN_KW_CASE:
		return "TOKEN_KW_CASE";
	case TOKEN_KW_CHAR:
		return "TOKEN_KW_CHAR";
	case TOKEN_KW_CONST:
		return "TOKEN_KW_CONST";
	case TOKEN_KW_CONTINUE:
		return "TOKEN_KW_CONTINUE";
	case TOKEN_KW_DEFAULT:
		return "TOKEN_KW_DEFAULT";
	case TOKEN_KW_DO:
		return "TOKEN_KW_DO";
	case TOKEN_KW_DOUBLE:
		return "TOKEN_KW_DOUBLE";
	case TOKEN_KW_ELSE:
		return "TOKEN_KW_ELSE";
	case TOKEN_KW_ENUM:
		return "TOKEN_KW_ENUM";
	case TOKEN_KW_EXTERN:
		return "TOKEN_KW_EXTERN";
	case TOKEN_KW_FALSE:
		return "TOKEN_KW_FALSE";
	case TOKEN_KW_TRUE:
		return "TOKEN_KW_TRUE";
	case TOKEN_KW_FLOAT:
		return "TOKEN_KW_FLOAT";
	case TOKEN_KW_FOR:
		return "TOKEN_KW_FOR";
	case TOKEN_KW_GENERIC:
		return "TOKEN_KW_GENERIC";
	case TOKEN_KW_GOTO:
		return "TOKEN_KW_GOTO";
	case TOKEN_KW_IF:
		return "TOKEN_KW_IF";
	case TOKEN_KW_INLINE:
		return "TOKEN_KW_INLINE";
	case TOKEN_KW_INT:
		return "TOKEN_KW_INT";
	case TOKEN_KW_IMAGINARY:
		return "TOKEN_KW_IMAGINARY";
	case TOKEN_KW_LONG:
		return "TOKEN_KW_LONG";
	case TOKEN_KW_NORETURN:
		return "TOKEN_KW_NORETURN";
	case TOKEN_KW_REGISTER:
		return "TOKEN_KW_REGISTER";
	case TOKEN_KW_RESTRICT:
		return "TOKEN_KW_RESTRICT";
	case TOKEN_KW_RETURN:
		return "TOKEN_KW_RETURN";
	case TOKEN_KW_SHORT:
		return "TOKEN_KW_SHORT";
	case TOKEN_KW_SIGNED:
		return "TOKEN_KW_SIGNED";
	case TOKEN_KW_SIZEOF:
		return "TOKEN_KW_SIZEOF";
	case TOKEN_KW_STATIC:
		return "TOKEN_KW_STATIC";
	case TOKEN_KW_STRUCT:
		return "TOKEN_KW_STRUCT";
	case TOKEN_KW_SWITCH:
		return "TOKEN_KW_SWITCH";
	case TOKEN_KW_THREAD_LOCAL:
		return "TOKEN_KW_THREAD_LOCAL";
	case TOKEN_KW_UNION:
		return "TOKEN_KW_UNION";
	case TOKEN_KW_UNSIGNED:
		return "TOKEN_KW_UNSIGNED";
	case TOKEN_KW_VOID:
		return "TOKEN_KW_VOID";
	case TOKEN_KW_VOLATILE:
		return "TOKEN_KW_VOLATILE";
	case TOKEN_KW_WHILE:
		return "TOKEN_KW_WHILE";
	case TOKEN_KW__BOOL:
		return "TOKEN_KW__BOOL";
	case TOKEN_KW__COMPLEX:
		return "TOKEN_KW__COMPLEX";
	case TOKEN_KW__STATIC_ASSERT:
		return "TOKEN_KW__STATIC_ASSERT";
	case TOKEN_KW__BITINT:
		return "TOKEN_KW__BITINT";
	case TOKEN_KW__PRAGMA:
		return "TOKEN_KW__PRAGMA";

	case TOKEN_KW___INT128:
		return "TOKEN_KW___INT128";

	case TOKEN_KW__DECIMAL32:
		return "TOKEN_KW__DECIMAL32";
	case TOKEN_KW__DECIMAL64:
		return "TOKEN_KW__DECIMAL64";
	case TOKEN_KW__DECIMAL128:
		return "TOKEN_KW__DECIMAL128";

	case TOKEN_KW__FLOAT32:
		return "TOKEN_KW__FLOAT32";
	case TOKEN_KW__FLOAT64:
		return "TOKEN_KW__FLOAT64";
	case TOKEN_KW__FLOAT80:
		return "TOKEN_KW__FLOAT80";
	case TOKEN_KW__FLOAT128:
		return "TOKEN_KW__FLOAT128";

	case TOKEN_KW___CONST__:
		return "TOKEN_KW___CONST__";
	case TOKEN_KW___SIGNED__:
		return "TOKEN_KW___SIGNED__";
	case TOKEN_KW___INLINE__:
		return "TOKEN_KW___INLINE__";
	case TOKEN_KW___RESTRICT__:
		return "TOKEN_KW___RESTRICT__";
	case TOKEN_KW___VOLATILE__:
		return "TOKEN_KW___VOLATILE__";
	}
	return "<unknown kind>";
}

const char *int_base_name(int b) {
	switch (b) {
	case TOKEN_INT_BASE_NONE:
		return "none";
	case TOKEN_INT_BASE_10:
		return "10";
	case TOKEN_INT_BASE_16:
		return "16";
	case TOKEN_INT_BASE_8:
		return "8";
	case TOKEN_INT_BASE_2:
		return "2";
	}
	return "?";
}

const char *float_style_name(int s) {
	switch (s) {
	case TOKEN_FLOAT_DEC:
		return "dec";
	case TOKEN_FLOAT_HEX:
		return "hex";
	}
	return "?";
}

const char *float_suf_name(int s) {
	switch (s) {
	case TOKEN_FSUF_NONE:
		return "none";
	case TOKEN_FSUF_f:
		return "f";
	case TOKEN_FSUF_l:
		return "l";
	case TOKEN_FSUF_f16:
		return "f16";
	case TOKEN_FSUF_f32:
		return "f32";
	case TOKEN_FSUF_f64:
		return "f64";
	case TOKEN_FSUF_f128:
		return "f128";
	case TOKEN_FSUF_f32x:
		return "f32x";
	case TOKEN_FSUF_f64x:
		return "f64x";
	case TOKEN_FSUF_f128x:
		return "f128x";
	case TOKEN_FSUF_df:
		return "df";
	case TOKEN_FSUF_dd:
		return "dd";
	case TOKEN_FSUF_dl:
		return "dl";
	}
	return "?";
}

static void dump_flags_inner(unsigned f, char *out, size_t n) {
	size_t off = 0;
#define APPEND(bit, name)                                                                                              \
	do {                                                                                                               \
		if (f & (bit))                                                                                                 \
			off += (size_t)snprintf(out + (off < n ? off : n), (off < n ? n - off : 0), "%s%s", (off ? "|" : ""),      \
									(name));                                                                           \
	} while (0)
	APPEND(TOKEN_FLAG_UNSIGNED, "U");
	APPEND(TOKEN_FLAG_SIZE_LONG, "L");
	APPEND(TOKEN_FLAG_SIZE_LONG_LONG, "LL");
	APPEND(TOKEN_FLAG_STR_PLAIN, "S:plain");
	APPEND(TOKEN_FLAG_STR_UTF8, "S:utf8");
	APPEND(TOKEN_FLAG_STR_UTF16, "S:utf16");
	APPEND(TOKEN_FLAG_STR_UTF32, "S:utf32");
	APPEND(TOKEN_FLAG_STR_WIDE, "S:wide");
#undef APPEND
	if (off == 0 && n) {
		out[0] = '-';
		if (n > 1)
			out[1] = '\0';
	}
}

void dump_flags(unsigned f, char *out, size_t n) {
	if (n == 0)
		return;
	out[0] = '\0';
	dump_flags_inner(f, out, n);
}

void dump_span(const struct source_span *sp) {
	fprintf(stderr, "    span: %s:%ld:%ld -> %s:%ld:%ld (offs %zu..%zu)\n",
			sp->start.filename ? sp->start.filename : "(null)", sp->start.line, sp->start.column,
			sp->end.filename ? sp->end.filename : "(null)", sp->end.line, sp->end.column, sp->start.offset,
			sp->end.offset);
}

static size_t c8len(const char8_t *s) {
	size_t n = 0;
	while (s && s[n] != '\0')
		n++;
	return n;
}
static size_t u16len(const char16_t *s) {
	size_t n = 0;
	while (s && s[n] != u'\0')
		n++;
	return n;
}
static size_t u32len(const char32_t *s) {
	size_t n = 0;
	while (s && s[n] != U'\0')
		n++;
	return n;
}

static size_t count_embedded_nuls_u8(const char8_t *s) {
	size_t n = 0;
	if (!s)
		return 0;
	for (size_t i = 0; s[i] != '\0'; ++i)
		if (s[i] == 0)
			++n;
	return n;
}
static size_t count_embedded_nuls_u16(const char16_t *s) {
	size_t n = 0;
	if (!s)
		return 0;
	for (size_t i = 0; s[i] != u'\0'; ++i)
		if (s[i] == 0)
			++n;
	return n;
}
static size_t count_embedded_nuls_u32(const char32_t *s) {
	size_t n = 0;
	if (!s)
		return 0;
	for (size_t i = 0; s[i] != U'\0'; ++i)
		if (s[i] == 0)
			++n;
	return n;
}
static size_t count_embedded_nuls_w(const wchar_t *s) {
	size_t n = 0;
	if (!s)
		return 0;
	for (size_t i = 0; s[i] != L'\0'; ++i)
		if (s[i] == 0)
			++n;
	return n;
}

static void c_escape_preview(const char *s, char *out, size_t max_out, size_t max_chars) {
	if (!out || max_out == 0)
		return;
	size_t o = 0, shown = 0;
#define EMIT(ch)                                                                                                       \
	do {                                                                                                               \
		if (o + 1 < max_out)                                                                                           \
			out[o++] = (ch);                                                                                           \
	} while (0)
#define EMIT2(a, b)                                                                                                    \
	do {                                                                                                               \
		if (o + 2 < max_out) {                                                                                         \
			out[o++] = (a);                                                                                            \
			out[o++] = (b);                                                                                            \
		}                                                                                                              \
	} while (0)
	for (; s && *s && shown < max_chars; ++s, ++shown) {
		unsigned char c = (unsigned char)*s;
		switch (c) {
		case '\n':
			EMIT2('\\', 'n');
			break;
		case '\r':
			EMIT2('\\', 'r');
			break;
		case '\t':
			EMIT2('\\', 't');
			break;
		case '\v':
			EMIT2('\\', 'v');
			break;
		case '\f':
			EMIT2('\\', 'f');
			break;
		case '\\':
			EMIT2('\\', '\\');
			break;
		case '\"':
			EMIT2('\\', '\"');
			break;
		default:
			if (c < 0x20 || c == 0x7F) {
				int n = 0;
				if (o + 4 < max_out)
					n = snprintf(out + o, max_out - o, "\\x%02X", c);
				o += (n > 0 ? (size_t)n : 0);
			} else {
				EMIT((char)c);
			}
		}
	}
	if (s && *s) {
		EMIT('.');
		EMIT('.');
		EMIT('.');
	}
	if (o < max_out)
		out[o] = '\0';
#undef EMIT
#undef EMIT2
}

static void dump_hex_u8(const char8_t *p, size_t n, size_t max_units) {
	fprintf(stderr, "  units8 :");
	for (size_t i = 0; i < n && i < max_units; i++)
		fprintf(stderr, " %02X", (unsigned)p[i]);
	if (n > max_units)
		fprintf(stderr, " ...");
	fprintf(stderr, "\n");
}
static void dump_hex_u16(const char16_t *p, size_t n, size_t max_units) {
	fprintf(stderr, "  units16:");
	for (size_t i = 0; i < n && i < max_units; i++)
		fprintf(stderr, " %04X", (unsigned)p[i]);
	if (n > max_units)
		fprintf(stderr, " ...");
	fprintf(stderr, "\n");
}
static void dump_hex_u32(const char32_t *p, size_t n, size_t max_units) {
	fprintf(stderr, "  units32:");
	for (size_t i = 0; i < n && i < max_units; i++)
		fprintf(stderr, " %08X", (unsigned)p[i]);
	if (n > max_units)
		fprintf(stderr, " ...");
	fprintf(stderr, "\n");
}
static void dump_hex_w(const wchar_t *p, size_t n, size_t max_units) {
	fprintf(stderr, "  unitsW :");
	for (size_t i = 0; i < n && i < max_units; i++)
		fprintf(stderr, sizeof(wchar_t) == 2 ? " %04X" : " %08lX", (unsigned long)p[i]);
	if (n > max_units)
		fprintf(stderr, " ...");
	fprintf(stderr, "\n");
}

static void utf16_surrogate_checks(const char16_t *s, size_t n) {
	size_t lonely_hi = 0, lonely_lo = 0;
	for (size_t i = 0; i < n; i++) {
		uint16_t cu = (uint16_t)s[i];
		if (cu >= 0xD800 && cu <= 0xDBFF) {
			if (!(i + 1 < n)) {
				lonely_hi++;
				break;
			}
			uint16_t cu2 = (uint16_t)s[i + 1];
			if (!(cu2 >= 0xDC00 && cu2 <= 0xDFFF))
				lonely_hi++;
			else
				i++;
		} else if (cu >= 0xDC00 && cu <= 0xDFFF) {
			lonely_lo++;
		}
	}
	if (lonely_hi || lonely_lo)
		fprintf(stderr, "  warn: utf16-surrogates: lonely_hi=%zu lonely_lo=%zu\n", lonely_hi, lonely_lo);
}

static void check_string_flag_exclusivity(unsigned f) {
	unsigned kinds = 0;
	if (f & TOKEN_FLAG_STR_PLAIN)
		kinds++;
	if (f & TOKEN_FLAG_STR_UTF8)
		kinds++;
	if (f & TOKEN_FLAG_STR_UTF16)
		kinds++;
	if (f & TOKEN_FLAG_STR_UTF32)
		kinds++;
	if (f & TOKEN_FLAG_STR_WIDE)
		kinds++;
	if (kinds != 1)
		fprintf(stderr, "  warn: string-kind flags: expected exactly one, have %u\n", kinds);
}

static const char *str_prefix(unsigned f) {
	if (f & TOKEN_FLAG_STR_UTF8)
		return "u8";
	if (f & TOKEN_FLAG_STR_UTF16)
		return "u";
	if (f & TOKEN_FLAG_STR_UTF32)
		return "U";
	if (f & TOKEN_FLAG_STR_WIDE)
		return "L";
	return "";
}

void dump_token(const struct token *t, const char *label) {
	char flags[128] = {0};
	dump_flags(t->flags, flags, sizeof(flags));

	fprintf(stderr, "%s {\n", label ? label : "token");
	fprintf(stderr, "  kind: %s (%d)\n", token_kind_name(t->kind), t->kind);
	fprintf(stderr, "  flags: %s\n", flags);

	switch (t->kind) {
	case TOKEN_IDENTIFIER:
	case TOKEN_HEADER_NAME:
		fprintf(stderr, "  str: \"%s\"\n", t->val.str ? t->val.str : "(null)");
		break;

	case TOKEN_STRING_LITERAL: {
		check_string_flag_exclusivity(t->flags);
		fprintf(stderr, "  string: prefix=%s\"...\"\n", str_prefix(t->flags));

		char prev[96];
		if (t->flags & (TOKEN_FLAG_STR_PLAIN | TOKEN_FLAG_STR_UTF8)) {
			const char *p =
				(t->flags & TOKEN_FLAG_STR_PLAIN) ? (const char *)t->val.str_lit : (const char *)t->val.str_lit;
			c_escape_preview(p ? p : "", prev, sizeof(prev), 72);
			fprintf(stderr, "  preview: \"%s\"\n", prev);

			if (t->flags & TOKEN_FLAG_STR_PLAIN) {
				size_t n = t->val.str_lit ? strlen(t->val.str_lit) : 0;
				size_t nul = 0;
				fprintf(stderr, "  length(bytes): %zu, embedded_nuls=%zu\n", n, nul);
				dump_hex_u8((const char8_t *)t->val.str_lit, n, 32);
			} else {
				const char8_t *u8 = (t->val.str8_lit);
				size_t n = u8 ? c8len(u8) : 0;
				size_t nul = count_embedded_nuls_u8(u8);
				fprintf(stderr, "  length(u8): %zu, embedded_nuls=%zu\n", n, nul);
				dump_hex_u8(u8, n, 32);
			}
		} else if (t->flags & TOKEN_FLAG_STR_UTF16) {
			size_t n = t->val.str16_lit ? u16len(t->val.str16_lit) : 0;
			size_t nul = count_embedded_nuls_u16(t->val.str16_lit);
			fprintf(stderr, "  length(u16): %zu, embedded_nuls=%zu\n", n, nul);
			dump_hex_u16(t->val.str16_lit, n, 24);
			utf16_surrogate_checks(t->val.str16_lit, n);
		} else if (t->flags & TOKEN_FLAG_STR_UTF32) {
			size_t n = t->val.str32_lit ? u32len(t->val.str32_lit) : 0;
			size_t nul = count_embedded_nuls_u32(t->val.str32_lit);
			fprintf(stderr, "  length(u32): %zu, embedded_nuls=%zu\n", n, nul);
			dump_hex_u32(t->val.str32_lit, n, 16);
		} else if (t->flags & TOKEN_FLAG_STR_WIDE) {
			size_t n = t->val.wstr_lit ? wcslen(t->val.wstr_lit) : 0;
			size_t nul = count_embedded_nuls_w(t->val.wstr_lit);
			fprintf(stderr, "  length(wchar_t): %zu, embedded_nuls=%zu (wchar_t=%zu bytes)\n", n, nul, sizeof(wchar_t));
			dump_hex_w(t->val.wstr_lit, n, 24);
		} else {
			fprintf(stderr, "  string: (unknown encoding)\n");
		}
	} break;

	case TOKEN_CHARACTER_CONSTANT:
		check_string_flag_exclusivity(t->flags);
		if (t->flags & TOKEN_FLAG_STR_PLAIN) {
			fprintf(stderr, "  char: 0x%02X ('", (unsigned char)t->val.c);
			char prev[8];
			c_escape_preview((char[]){t->val.c ? t->val.c : '\0', 0}, prev, sizeof(prev), 1);
			fprintf(stderr, "%s')\n", prev);
		} else if (t->flags & TOKEN_FLAG_STR_UTF8) {
			fprintf(stderr, "  char8: U+%04X\n", (unsigned)t->val.c8);
		} else if (t->flags & TOKEN_FLAG_STR_UTF16) {
			fprintf(stderr, "  char16: U+%04X\n", (unsigned)t->val.c16);
		} else if (t->flags & TOKEN_FLAG_STR_UTF32) {
			fprintf(stderr, "  char32: U+%04X\n", (unsigned)t->val.c32);
		} else if (t->flags & TOKEN_FLAG_STR_WIDE) {
			fprintf(stderr, "  wchar: U+%04X\n", (unsigned)t->val.wc);
		} else {
			fprintf(stderr, "  char: (unknown encoding)\n");
		}
		break;

	case TOKEN_INTEGER_CONSTANT:
		fprintf(stderr, "  int: %s=%" PRId64 " (u=%" PRIu64 ")\n", int_base_name(t->num_extra.i.base),
				(int64_t)t->val.i, (uint64_t)t->val.u);
		break;

	case TOKEN_FLOATING_CONSTANT:
		fprintf(stderr, "  float: %s, suf=%s, val=%.17g\n", float_style_name(t->num_extra.f.style),
				float_suf_name(t->num_extra.f.suffix), t->val.f);
		break;

	case TOKEN_ERROR:
		fprintf(stderr, "  error: \"%s\"\n", t->val.err ? t->val.err : "(null)");
		break;

	default:
		break;
	}

	dump_span(&t->loc);
	fprintf(stderr, "}\n");
}
