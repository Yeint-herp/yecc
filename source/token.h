#ifndef TOKEN_H
#define TOKEN_H

#include <stdbool.h>
#include <stdint.h>
#include <streamer.h>
#include <wchar.h>

enum token_kind {
    TOKEN_ERROR = -1,
    TOKEN_EOF = 0,

    TOKEN_IDENTIFIER,
    TOKEN_INTEGER_CONSTANT,
    TOKEN_FLOATING_CONSTANT,
    TOKEN_CHARACTER_CONSTANT,
    TOKEN_STRING_LITERAL,

    TOKEN_LPAREN,    // (
    TOKEN_RPAREN,    // )
    TOKEN_LBRACKET,  // [
    TOKEN_RBRACKET,  // ]
    TOKEN_LBRACE,    // {
    TOKEN_RBRACE,    // }
    TOKEN_PERIOD,    // .
    TOKEN_ELLIPSIS,  // ...
    TOKEN_ARROW,     // ->

    TOKEN_PLUS,           // +
    TOKEN_PLUS_PLUS,      // ++
    TOKEN_MINUS,          // -
    TOKEN_MINUS_MINUS,    // --
    TOKEN_STAR,           // *
    TOKEN_SLASH,          // /
    TOKEN_PERCENT,        // %

    TOKEN_LT,             // <
    TOKEN_GT,             // >
    TOKEN_LE,             // <=
    TOKEN_GE,             // >=
    TOKEN_EQEQ,           // ==
    TOKEN_NEQ,            // !=

    TOKEN_AMP,            // &
    TOKEN_AND_AND,        // &&
    TOKEN_PIPE,           // |
    TOKEN_OR_OR,          // ||
    TOKEN_CARET,          // ^
    TOKEN_TILDE,          // ~
    TOKEN_EXCLAMATION,    // !

    TOKEN_QUESTION,       // ?
    TOKEN_COLON,          // :
    TOKEN_SEMICOLON,      // ;
    TOKEN_COMMA,          // ,

    TOKEN_ASSIGN,           // =
    TOKEN_PLUS_ASSIGN,      // +=
    TOKEN_MINUS_ASSIGN,     // -=
    TOKEN_STAR_ASSIGN,      // *=
    TOKEN_SLASH_ASSIGN,     // /=
    TOKEN_PERCENT_ASSIGN,   // %=
    TOKEN_LSHIFT_ASSIGN,    // <<=
    TOKEN_RSHIFT_ASSIGN,    // >>=
    TOKEN_AMP_ASSIGN,       // &=
    TOKEN_XOR_ASSIGN,       // ^=
    TOKEN_OR_ASSIGN,        // |=

    TOKEN_KW_TYPEOF,
    TOKEN_KW_ASM,
    TOKEN_KW___ASM__,
    TOKEN_KW___ATTRIBUTE__,
    TOKEN_KW___BUILTIN_TYPES_COMPATIBLE_P,
    TOKEN_KW___AUTO_TYPE,
    TOKEN_KW___EXTENSION__,
    TOKEN_KW___LABEL__,
    TOKEN_KW___REAL__,
    TOKEN_KW___IMAG__,
    TOKEN_KW___THREAD,
    TOKEN_KW___FUNCTION__,

    TOKEN_KW_ALIGNAS,
    TOKEN_KW_ALIGNOF,
    TOKEN_KW_ATOMIC,
    TOKEN_KW_AUTO,
    TOKEN_KW_BOOL,
    TOKEN_KW_BREAK,
    TOKEN_KW_CASE,
    TOKEN_KW_CHAR,
    TOKEN_KW_CONST,
    TOKEN_KW_CONTINUE,
    TOKEN_KW_DEFAULT,
    TOKEN_KW_DO,
    TOKEN_KW_DOUBLE,
    TOKEN_KW_ELSE,
    TOKEN_KW_ENUM,
    TOKEN_KW_EXTERN,
    TOKEN_KW_FALSE,
    TOKEN_KW_FLOAT,
    TOKEN_KW_FOR,
    TOKEN_KW_GENERIC,
    TOKEN_KW_GOTO,
    TOKEN_KW_IF,
    TOKEN_KW_INLINE,
    TOKEN_KW_INT,
    TOKEN_KW_IMAGINARY,
    TOKEN_KW_LONG,
    TOKEN_KW_NORETURN,
    TOKEN_KW_REGISTER,
    TOKEN_KW_RESTRICT,
    TOKEN_KW_RETURN,
    TOKEN_KW_SHORT,
    TOKEN_KW_SIGNED,
    TOKEN_KW_SIZEOF,
    TOKEN_KW_STATIC,
    TOKEN_KW_STRUCT,
    TOKEN_KW_SWITCH,
    TOKEN_KW_THREAD_LOCAL,
    TOKEN_KW_UNION,
    TOKEN_KW_UNSIGNED,
    TOKEN_KW_VOID,
    TOKEN_KW_VOLATILE,
    TOKEN_KW_WHILE,
    TOKEN_KW__BOOL,
    TOKEN_KW__COMPLEX,
    TOKEN_KW__STATIC_ASSERT,

    TOKEN_KW___INT128,

    TOKEN_KW__DECIMAL32,
    TOKEN_KW__DECIMAL64,
    TOKEN_KW__DECIMAL128,

    TOKEN_KW__FLOAT16,
    TOKEN_KW__FLOAT32,
    TOKEN_KW__FLOAT64,
    TOKEN_KW__FLOAT128,

    TOKEN_KW__ACCUM,
    TOKEN_KW__UACCUM,
    TOKEN_KW__SAT_ACCUM,
    TOKEN_KW__USAT_ACCUM,
    TOKEN_KW__FRACT,
    TOKEN_KW__UFRACT,
    TOKEN_KW__SAT_FRACT,
    TOKEN_KW__USAT_FRACT,

    TOKEN_KW___TRANSACTION_ATOMIC,
    TOKEN_KW___TRANSACTION_RELAXED,
    TOKEN_KW___TRANSACTION_ACQUIRE,
    TOKEN_KW___TRANSACTION_RELEASE,

    TOKEN_KW___CONST__,
    TOKEN_KW___SIGNED__,
    TOKEN_KW___INLINE__,
    TOKEN_KW___RESTRICT__,
    TOKEN_KW___VOLATILE__,
};

/* flags on numeric/string tokens (suffixes, wide/text markers) */
enum token_flags {
	TOKEN_FLAG_NONE = 0,
	TOKEN_FLAG_TEXT_LONG = 1u << 0,		// L"" or L''
	TOKEN_FLAG_UNSIGNED = 1u << 1,		// u/U
	TOKEN_FLAG_SIZE_LONG = 1u << 2,		// L
	TOKEN_FLAG_SIZE_LONG_LONG = 1u << 3 // LL
};

/* the payload carried by a token */
union token_value {
	int64_t i;		 // integer literal
	double f;		 // floating-point literal
	const char *str; // identifier/keyword (interned)
	char *str_lit;	 // string literal contents (malloc’d)
	const char *err; // error message
	char c;			 // char literal
	wchar_t wc;		 // wide char literal
};

/* The fundamental token structure. */
struct token {
	enum token_kind kind;
	struct source_span loc;
	union token_value val;
	enum token_flags flags;
};

#endif /* TOKEN_H */
