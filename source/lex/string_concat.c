#include "base/vector.h"
#include <lex/string_concat.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static inline unsigned ctx_wchar_bits(const struct yecc_context *ctx) {
	return (ctx && ctx->wchar_bits) ? ctx->wchar_bits : 32;
}

/* Literal kind "metadata" used for ranking/promotion decisions. */
struct lit_info {
	enum lit_kind kind;
	unsigned rank;		/* Promotion rank: plain < u8 < u < U < L (contextual) */
	const char *name;	/* For diagnostics: "plain", "u8", "u", "U", "L" */
	unsigned unit_bits; /* Code unit width in bits: 8 / 16 / 32 / ctx-dependent for L */
};

/* Map kind -> lit_info. */
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
		return (struct lit_info){k, 4, "L", ctx_wchar_bits(ctx)};
	}

	/* defensive default */
	return (struct lit_info){LIT_PLAIN, 0, "plain", 8};
}

/*
 * Promotion rule:
 *  - Choose the higher rank.
 *  - never narrow relative to the widest input unit size.
 *    If result.unit_bits < max(input.unit_bits), we bump up to a kind that
 *    can represent the widest input. Specifically, if any input is 32-bit,
 *    result will be 32-bit-capable (UTF-32) even if L is 16-bit on this target (UEFI/Windows).
 *
 * This preserves code points and prevents silent data loss (surrogate pairs collapsing).
 */
static inline enum lit_kind lit_promote(enum lit_kind a, enum lit_kind b, const struct yecc_context *ctx) {
	struct lit_info A = lit_info_of(a, ctx), B = lit_info_of(b, ctx);

	/* Pick by rank */
	enum lit_kind r = (A.rank >= B.rank) ? a : b;
	struct lit_info R = lit_info_of(r, ctx);

	/* Enforce non-narrowing with respect to input unit widths. */
	unsigned need_bits = (A.unit_bits >= B.unit_bits) ? A.unit_bits : B.unit_bits;

	/* If the chosen result would have smaller code units than required, fix it. */
	if (R.unit_bits < need_bits) {
		if (need_bits >= 32) {
			r = LIT_UTF32;
		} else if (need_bits == 16) {
			/* shouldn't happen */
			r = LIT_UTF16;
		}
	}
	return r;
}

/* Promotion diagnostics: emitted when a/b widen to reach 'to'. */
static inline void diag_promotion(struct yecc_context *ctx, struct source_span sp, enum lit_kind from,
								  enum lit_kind to) {
	if (from == to)
		return;
	if (!yecc_context_warning_enabled(ctx, YECC_W_STRING_WIDTH_PROMOTION))
		return;

	if (ctx->warnings_as_errors && yecc_context_warning_as_error(ctx, YECC_W_STRING_WIDTH_PROMOTION)) {
		diag_error(sp, "string literal concatenation promotes from %s to %s", lit_info_of(from, ctx).name,
				   lit_info_of(to, ctx).name);
	} else {
		diag_warning(sp, "string literal concatenation promotes from %s to %s", lit_info_of(from, ctx).name,
					 lit_info_of(to, ctx).name);
	}
}

/* Token flags -> literal kind. */
static enum lit_kind tok_kind_from_flags(const struct token *t, const struct yecc_context *ctx) {
	(void)ctx;
	if (t->flags & TOKEN_FLAG_STR_UTF32)
		return LIT_UTF32;
	if (t->flags & TOKEN_FLAG_STR_UTF16)
		return LIT_UTF16;
	if (t->flags & TOKEN_FLAG_STR_UTF8)
		return LIT_UTF8;
	if (t->flags & TOKEN_FLAG_STR_WIDE)
		return LIT_WIDE;
	return LIT_PLAIN;
}

/* Encode a Unicode scalar value to UTF-8. Invalid scalars become U+FFFD. */
static void u8_append(void *ptr, uint32_t cp) {
	vector_of(char) *out = ptr;
	if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
		cp = 0xFFFD;
	if (cp <= 0x7F) {
		vector_push(out, (char)cp);
	} else if (cp <= 0x7FF) {
		vector_push(out, (char)(0xC0 | (cp >> 6)));
		vector_push(out, (char)(0x80 | (cp & 0x3F)));
	} else if (cp <= 0xFFFF) {
		vector_push(out, (char)(0xE0 | (cp >> 12)));
		vector_push(out, (char)(0x80 | ((cp >> 6) & 0x3F)));
		vector_push(out, (char)(0x80 | (cp & 0x3F)));
	} else {
		vector_push(out, (char)(0xF0 | (cp >> 18)));
		vector_push(out, (char)(0x80 | ((cp >> 12) & 0x3F)));
		vector_push(out, (char)(0x80 | ((cp >> 6) & 0x3F)));
		vector_push(out, (char)(0x80 | (cp & 0x3F)));
	}
}

/* Encode to UTF-16 with surrogate pairs where needed. */
static void u16_append(void *ptr, uint32_t cp) {
	vector_of(char16_t) *out = ptr;
	if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
		cp = 0xFFFD;
	if (cp <= 0xFFFF) {
		vector_push(out, (char16_t)cp);
	} else {
		cp -= 0x10000;
		vector_push(out, (char16_t)(0xD800 + (cp >> 10)));
		vector_push(out, (char16_t)(0xDC00 + (cp & 0x3FF)));
	}
}

/* Encode to UTF-32 (one code point per code unit). */
static void u32_append(void *ptr, uint32_t cp) {
	vector_of(char32_t) *out = ptr;
	if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
		cp = 0xFFFD;
	vector_push(out, (char32_t)cp);
}

/*
 * Encode to wchar_t* according to ctx->wchar_bits.
 *  - 8  bits: lossy clamp to 0..255 (invalid -> U+FFFD -> 0xFD when clamped)
 *  - 16 bits: UTF-16 logic with surrogate pairs
 *  - 32 bits: UTF-32 logic
 */
static void w_append(struct yecc_context *ctx, void *ptr, uint32_t cp) {
	vector_of(wchar_t) *out = ptr;
	unsigned wb = ctx_wchar_bits(ctx);
	if (wb == 8) {
		vector_push(out, (wchar_t)((cp > 0xFF) ? 0xFFFD : (cp & 0xFF)));
	} else if (wb == 16) {
		if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
			cp = 0xFFFD;
		if (cp <= 0xFFFF)
			vector_push(out, (wchar_t)cp);
		else {
			cp -= 0x10000;
			vector_push(out, (wchar_t)(0xD800 + (cp >> 10)));
			vector_push(out, (wchar_t)(0xDC00 + (cp & 0x3FF)));
		}
	} else { /* 32-bit wchar_t */
		if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
			cp = 0xFFFD;
		vector_push(out, (wchar_t)cp);
	}
}

/*
 * Forward-progress UTF-8 decoder:
 *  - Rejects overlong encodings, surrogates, > U+10FFFF, malformed tails.
 *  - On any error, advances past the offending head byte and yields U+FFFD.
 */
static bool utf8_next(const char **p, uint32_t *out) {
	const unsigned char *s = (const unsigned char *)(*p);
	unsigned char b0 = s[0];
	if (b0 == 0)
		return false;
	uint32_t cp;
	size_t need;
	if (b0 <= 0x7F) {
		cp = b0;
		need = 1;
	} else if ((b0 & 0xE0) == 0xC0) {
		cp = b0 & 0x1F;
		need = 2;
	} else if ((b0 & 0xF0) == 0xE0) {
		cp = b0 & 0x0F;
		need = 3;
	} else if ((b0 & 0xF8) == 0xF0) {
		cp = b0 & 0x07;
		need = 4;
	} else {
		(*p)++;
		*out = 0xFFFD;
		return true;
	}
	for (size_t i = 1; i < need; i++) {
		unsigned char c = s[i];
		if ((c & 0xC0) != 0x80) {
			*p += i;
			*out = 0xFFFD;
			return true;
		}
		cp = (cp << 6) | (c & 0x3F);
	}
	*p += need;
	if ((need == 2 && cp < 0x80) || (need == 3 && cp < 0x800) || (need == 4 && cp < 0x10000) ||
		(cp >= 0xD800 && cp <= 0xDFFF) || (cp > 0x10FFFF))
		cp = 0xFFFD;
	*out = cp;
	return true;
}

/* UTF-16 decoder with surrogate-pair handling and U+FFFD on anomalies. */
static bool utf16_next(const char16_t **p, uint32_t *out) {
	char16_t w1 = **p;
	if (w1 == 0)
		return false;
	(*p)++;
	if (w1 >= 0xD800 && w1 <= 0xDBFF) {
		char16_t w2 = **p;
		if (w2 >= 0xDC00 && w2 <= 0xDFFF) {
			(*p)++;
			*out = 0x10000 + (((uint32_t)(w1 - 0xD800) << 10) | (uint32_t)(w2 - 0xDC00));
			return true;
		}
		*out = 0xFFFD;
		return true;
	}
	if (w1 >= 0xDC00 && w1 <= 0xDFFF) {
		*out = 0xFFFD;
		return true;
	}
	*out = (uint32_t)w1;
	return true;
}

/* Callback for code points; lets us decouple decoding from the chosen sink. */
typedef void (*cp_sink)(uint32_t, void *);

/* Iterate literal payload as Unicode scalar values and feed a sink. */
static void for_each_cp_from_token(const struct token *t, const struct yecc_context *ctx, cp_sink cb, void *user) {
	enum lit_kind k = tok_kind_from_flags(t, ctx);
	switch (k) {
	case LIT_PLAIN: {
		/* Plain: 8-bit bytes treated as code points 0..255 (lossy). */
		const unsigned char *p = (const unsigned char *)t->val.str_lit;
		for (; *p; ++p)
			cb((uint32_t)(*p), user);
	} break;
	case LIT_UTF8: {
		const char *p = (const char *)t->val.str8_lit;
		uint32_t cp;
		while (*p)
			if (utf8_next(&p, &cp))
				cb(cp, user);
			else
				break;
	} break;
	case LIT_UTF16: {
		const char16_t *p = t->val.str16_lit;
		uint32_t cp;
		while (*p)
			if (utf16_next(&p, &cp))
				cb(cp, user);
			else
				break;
	} break;
	case LIT_UTF32: {
		const char32_t *p = t->val.str32_lit;
		for (; *p; ++p)
			cb((uint32_t)*p, user);
	} break;
	case LIT_WIDE: {
		/* Decode wide according to ctx, mirroring w_append in reverse. */
		unsigned wb = ctx_wchar_bits(ctx);
		if (wb == 8) {
			const unsigned char *p = (const unsigned char *)t->val.wstr_lit;
			for (; *p; ++p)
				cb((uint32_t)(*p), user);
		} else if (wb == 16) {
			const char16_t *p = (const char16_t *)t->val.wstr_lit;
			uint32_t cp;
			while (*p)
				if (utf16_next(&p, &cp))
					cb(cp, user);
				else
					break;
		} else {
			const char32_t *p = (const char32_t *)t->val.wstr_lit;
			for (; *p; ++p)
				cb((uint32_t)*p, user);
		}
	} break;
	}
}

/* Accumulator for building the promoted literal. */
struct build_ctx {
	enum lit_kind outk;		  /* Final kind chosen after promotion. */
	struct yecc_context *ctx; /* For wchar_t policy and diagnostics. */
	union {
		vector_of(char) u8;
		vector_of(char16_t) u16;
		vector_of(char32_t) u32;
		vector_of(wchar_t) w;
		vector_of(char) plain;
	} buf;
};

/* cp -> encoded unit(s) into the c1hosen buffer. */
static void sink_append(uint32_t cp, void *user) {
	struct build_ctx *bc = (struct build_ctx *)user;
	switch (bc->outk) {
	case LIT_UTF8:
		u8_append(&bc->buf.u8, cp);
		break;
	case LIT_UTF16:
		u16_append(&bc->buf.u16, cp);
		break;
	case LIT_UTF32:
		u32_append(&bc->buf.u32, cp);
		break;
	case LIT_WIDE:
		w_append(bc->ctx, &bc->buf.w, cp);
		break;
	case LIT_PLAIN:
		vector_push(&bc->buf.plain, (char)(cp & 0xFF));
		break;
	}
}

/* Finalize: terminate, move buffer into token, stamp flags & location. */
static void finalize_into_token(struct build_ctx *bc, struct token *out, struct source_span sp) {
	memset(out, 0, sizeof(*out));
	out->kind = TOKEN_STRING_LITERAL;
	out->loc = sp;

	switch (bc->outk) {
	case LIT_UTF8:
		vector_push(&bc->buf.u8, '\0');
		out->val.str8_lit = (char8_t *)bc->buf.u8.data;
		out->flags = TOKEN_FLAG_STR_UTF8;
		bc->buf.u8.data = nullptr; bc->buf.u8.size = 0; bc->buf.u8.capacity = 0;
		break;
	case LIT_UTF16:
		vector_push(&bc->buf.u16, 0);
		out->val.str16_lit = (char16_t *)bc->buf.u16.data;
		out->flags = TOKEN_FLAG_STR_UTF16;
		bc->buf.u16.data = nullptr; bc->buf.u16.size = 0; bc->buf.u16.capacity = 0;
		break;
	case LIT_UTF32:
		vector_push(&bc->buf.u32, 0);
		out->val.str32_lit = (char32_t *)bc->buf.u32.data;
		out->flags = TOKEN_FLAG_STR_UTF32;
		bc->buf.u32.data = nullptr; bc->buf.u32.size = 0; bc->buf.u32.capacity = 0;
		break;
	case LIT_WIDE:
		vector_push(&bc->buf.w, 0);
		out->val.wstr_lit = (wchar_t *)bc->buf.w.data;
		out->flags = TOKEN_FLAG_STR_WIDE;
		bc->buf.w.data = nullptr; bc->buf.w.size = 0; bc->buf.w.capacity = 0;
		break;
	case LIT_PLAIN:
		vector_push(&bc->buf.plain, '\0');
		out->val.str_lit = bc->buf.plain.data;
		out->flags = TOKEN_FLAG_STR_PLAIN;
		bc->buf.plain.data = nullptr; bc->buf.plain.size = 0; bc->buf.plain.capacity = 0;
		break;
	}
}

bool lex_concat_string_pair(struct yecc_context *ctx, const struct token *a, const struct token *b,
							struct source_span sp, struct token *out) {
	if (!token_is_string_lit(a) || !token_is_string_lit(b))
		return false;

	enum lit_kind ka = tok_kind_from_flags(a, ctx);
	enum lit_kind kb = tok_kind_from_flags(b, ctx);
	enum lit_kind k = lit_promote(ka, kb, ctx);

	if (k != ka)
		diag_promotion(ctx, sp, ka, k);
	if (k != kb)
		diag_promotion(ctx, sp, kb, k);

	struct build_ctx bc = {.outk = k, .ctx = ctx};
	memset(&bc.buf, 0, sizeof(bc.buf));

	/* Stream a -> cp -> sink; then b -> cp -> sink. */
	for_each_cp_from_token(a, ctx, sink_append, &bc);
	for_each_cp_from_token(b, ctx, sink_append, &bc);

	finalize_into_token(&bc, out, sp);
	return true;
}

void lex_concat_adjacent_string_literals(struct yecc_context *ctx, void *ptr) {
	vector_of(struct token) *v = ptr;
	if (!v || vector_size(v) == 0)
		return;

	vector_of(struct token) out = {};
	size_t i = 0, n = vector_size(v);
	vector_reserve(&out, n);

	while (i < n) {
		struct token cur = v->data[i];
		if (!token_is_string_lit(&cur)) {
			vector_push(&out, cur);
			++i;
			continue;
		}

		size_t j = i + 1;
		while (j < n && token_is_string_lit(&v->data[j]))
			++j;

		struct token acc = cur;
		for (size_t k = i + 1; k < j; ++k) {
			struct token merged = {};
			struct source_span sp = {.start = acc.loc.start, .end = v->data[k].loc.end};
			if (!lex_concat_string_pair(ctx, &acc, &v->data[k], sp, &merged)) {
				/* Shouldn’t happen since we tested token_is_string_lit, but be defensive. */
				vector_push(&out, acc);
				acc = v->data[k];
			} else {
				acc = merged;
			}
		}
		vector_push(&out, acc);
		i = j;
	}

	vector_move(v, &out);
}
