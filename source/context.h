#ifndef CONTEXT_H
#define CONTEXT_H

#include <stdbool.h>
#include <stddef.h>
#include <vector.h>

/* language standard the front-end should adhere to. */
enum yecc_lang_standard {
	YECC_LANG_C89,
	YECC_LANG_C99,
	YECC_LANG_C11,
	YECC_LANG_C17,
	YECC_LANG_C23,
};

/* backend optimization preference. */
/* FIXME: only O0 and O2 are in effect, others round up/down to O2 */
enum yecc_opt_level {
	YECC_O0 = 0, /* no optimizations */
	YECC_O1 = 1,
	YECC_O2 = 2,
	YECC_O3 = 3,
};

/* diagnostics / warnings enum */
enum yecc_warning {
	YECC_W_UNUSED = 0,
	YECC_W_UNUSED_PARAMETER,
	YECC_W_SHADOW,
	YECC_W_TRIGRAPHS,
	YECC_W_MULTICHAR_CHAR,
	YECC_W_TRUNCATION,
	YECC_W_SIGN_COMPARE,
	YECC_W_IMPLICIT_DECL, /* implicit function/extern */
	YECC_W_MISSING_PROTOTYPES,
	YECC_W_SWITCH_ENUM,
	YECC_W_FALLTHROUGH,
	YECC_W_FORMAT,
	YECC_W_VLA,
	YECC_W_STRICT_ALIASING, /* front-end notes only */
	YECC_W_PEDANTIC,
	YECC_W_UNREACHABLE_CODE,
	YECC_W_DEPRECATED,
	YECC_W_COUNT
};

/* stage at which the compilation finishe and output is presented to the user. */
enum yecc_target_stage {
	YECC_TARGET_STAGE_I,   /* preprocessed raw source */
	YECC_TARGET_STAGE_AST, /* ast text representation */
	YECC_TARGET_STAGE_IR,  /* ir text representation */
	YECC_TARGET_STAGE_ASM, /* generated assembly */
	YECC_TARGET_STAGE_OBJ, /* assembled object */
	YECC_TARGET_STAGE_EXE  /* linked executable */
};

enum yecc_color_mode { YECC_COLOR_AUTO, YECC_COLOR_ALWAYS, YECC_COLOR_NEVER };

/* how to respond to unkown Pragmas. */
enum yecc_pragma_policy { YECC_PRAGMA_IGNORE, YECC_PRAGMA_WARN, YECC_PRAGMA_ERROR };

/* float mode: full HW fp, soft-float codegen intent, or disabled (reject float types). */
/* this does not affect the usage of SIMD in optimizations! */
enum yecc_float_mode { YECC_FLOAT_FULL, YECC_FLOAT_SOFT, YECC_FLOAT_DISABLED };

enum yecc_reloc_model { YECC_RELOC_STATIC, YECC_RELOC_PIC, YECC_RELOC_PIE };
enum yecc_code_model { YECC_CODE_SMALL, YECC_CODE_MEDIUM, YECC_CODE_LARGE };

/* a tiny, portable CPU feature mask, most are going to be unneeded for a long time. */
enum yecc_cpu_feature {
	YECC_CPUFEAT_SSE2 = 0,
	YECC_CPUFEAT_SSE3 = 1,
	YECC_CPUFEAT_SSSE3 = 2,
	YECC_CPUFEAT_SSE41 = 3,
	YECC_CPUFEAT_SSE42 = 4,
	YECC_CPUFEAT_POPCNT = 5,
	YECC_CPUFEAT_AES = 6,
	YECC_CPUFEAT_AVX = 7,
	YECC_CPUFEAT_AVX2 = 8,
	YECC_CPUFEAT_BMI1 = 9,
	YECC_CPUFEAT_BMI2 = 10,
	YECC_CPUFEAT_FMA = 11,
	YECC_CPUFEAT_F16C = 12,
	YECC_CPUFEAT_COUNT
};

/* top-level compiler context */
struct yecc_context {
	enum yecc_lang_standard lang_std;
	enum yecc_opt_level opt_level;
	enum yecc_target_stage target_stage;

	/* target and environment */
	const char *target_triple;
	const char *sysroot; /* path or nullptr */

	/* search paths */
	vector_of(const char *) include_paths;		  /* -I user paths */
	vector_of(const char *) system_include_paths; /* -isystem paths */

	/* command line defined macros */
	vector_of(const char *) predefined_macros;

	/* custom warnings */
	unsigned warning_enabled_mask;
	unsigned warning_error_mask;

	enum yecc_color_mode color_mode;
	enum yecc_pragma_policy unknown_pragma_policy;
	bool warnings_as_errors;
	bool pedantic;		  /* treat non-conforming constructs as diagnostics */
	int max_errors;		  /* cap hard errors, default 20 */
	bool gnu_extensions;  /* std=gnuXX vs std=cXX */
	bool yecc_extensions; /* disabed by -pedantic and -no-yecc */
	bool no_short_enums;  /* force enums to int size */

	bool enable_trigraphs; /* map ??x to punctuators */

	enum yecc_float_mode float_mode;
	bool fast_math;	  /* allow reassoc/FTZ/contract? high-level intent */
	bool strict_ieee; /* prefer strict FP (overrides fast_math where relevant) */
	enum yecc_reloc_model reloc_model;
	enum yecc_code_model code_model;

	/* CPU strings for backends that accept them; also masks for quick checks. */
	const char *cpu;							  /* e.g. "x86-64-v2" or "znver3" */
	const char *tune;							  /* e.g. "generic" */
	unsigned long long cpu_features_enable_mask;  /* feature allow-list */
	unsigned long long cpu_features_disable_mask; /* explicit disable overrides */

	/* standard libraries and link policy */
	bool use_standard_includes; /* inject default system include set */
	bool nostdlib;				/* do not link startup files or stdlibs */
	bool nodefaultlibs;			/* clear default libs, only explicit -l */
	bool nostartfiles;			/* clear startup files but still link default libs */
	bool static_link;			/* prefer static linking */
	bool link_libc;				/* link libc (if linking) */
	bool link_libm;				/* link libm (if linking) */
	bool link_libcompilerrt;	/* link compiler runtime helper lib */
	const char *output_path;	/* -o (borrowed) */

	bool trace_lexer, trace_pp, trace_parser, trace_sema, trace_ir, trace_codegen;
};

static inline unsigned yecc_warning_bit(enum yecc_warning w) { return 1u << (unsigned)w; }
static inline unsigned long long yecc_cpufeat_bit(enum yecc_cpu_feature f) { return 1ull << (unsigned)f; }
static inline bool yecc_std_at_least(struct yecc_context *ctx, enum yecc_lang_standard need) {
	return (int)ctx->lang_std >= (int)need;
}
static inline bool yecc_context_warning_enabled(const struct yecc_context *ctx, enum yecc_warning wid) {
	return (ctx->warning_enabled_mask & yecc_warning_bit(wid)) != 0u;
}
static inline bool yecc_context_warning_as_error(const struct yecc_context *ctx, enum yecc_warning wid) {
	return (ctx->warning_error_mask & yecc_warning_bit(wid)) != 0u;
}

_Static_assert(YECC_W_COUNT <= 32, "warning mask is 32-bit; increase type size");

void yecc_context_init(struct yecc_context *ctx);
void yecc_context_destroy(struct yecc_context *ctx);

void yecc_context_set_lang_standard(struct yecc_context *ctx, enum yecc_lang_standard std);
void yecc_context_set_opt_level(struct yecc_context *ctx, enum yecc_opt_level lvl);
void yecc_context_set_target_stage(struct yecc_context *ctx, enum yecc_target_stage stg);

void yecc_context_set_target_triple(struct yecc_context *ctx, const char *triple);
void yecc_context_set_sysroot(struct yecc_context *ctx, const char *sysroot);

void yecc_context_set_color_mode(struct yecc_context *ctx, enum yecc_color_mode mode);
void yecc_context_set_warnings_as_errors(struct yecc_context *ctx, bool on);
void yecc_context_set_pedantic(struct yecc_context *ctx, bool on);
void yecc_context_set_max_errors(struct yecc_context *ctx, int n);
void yecc_context_set_unknown_pragma_policy(struct yecc_context *ctx, enum yecc_pragma_policy pol);

void yecc_context_set_gnu_extensions(struct yecc_context *ctx, bool on);
void yecc_context_set_yecc_extensions(struct yecc_context *ctx, bool on);
void yecc_context_set_no_short_enums(struct yecc_context *ctx, bool on);

void yecc_context_set_enable_trigraphs(struct yecc_context *ctx, bool on);

void yecc_context_set_float_mode(struct yecc_context *ctx, enum yecc_float_mode fm);
void yecc_context_set_fast_math(struct yecc_context *ctx, bool on);
void yecc_context_set_strict_ieee(struct yecc_context *ctx, bool on);
void yecc_context_set_reloc_model(struct yecc_context *ctx, enum yecc_reloc_model rm);
void yecc_context_set_code_model(struct yecc_context *ctx, enum yecc_code_model cm);

void yecc_context_set_cpu(struct yecc_context *ctx, const char *cpu);
void yecc_context_set_tune(struct yecc_context *ctx, const char *tune);
void yecc_context_enable_feature(struct yecc_context *ctx, enum yecc_cpu_feature f, bool on);
void yecc_context_disable_feature(struct yecc_context *ctx, enum yecc_cpu_feature f, bool on);

void yecc_context_set_use_standard_includes(struct yecc_context *ctx, bool on);
void yecc_context_set_nostdlib(struct yecc_context *ctx, bool on);
void yecc_context_set_nodefaultlibs(struct yecc_context *ctx, bool on);
void yecc_context_set_nostartfiles(struct yecc_context *ctx, bool on);
void yecc_context_set_static_link(struct yecc_context *ctx, bool on);
void yecc_context_set_output_path(struct yecc_context *ctx, const char *out);

void yecc_context_set_link_libc(struct yecc_context *ctx, bool on);
void yecc_context_set_link_libm(struct yecc_context *ctx, bool on);
void yecc_context_set_link_compilerrt(struct yecc_context *ctx, bool on);

void yecc_context_add_include_path(struct yecc_context *ctx, const char *path, bool system_path);
void yecc_context_add_define(struct yecc_context *ctx, const char *macro_eq_value);

void yecc_context_set_trace_lexer(struct yecc_context *ctx, bool on);
void yecc_context_set_trace_pp(struct yecc_context *ctx, bool on);
void yecc_context_set_trace_parser(struct yecc_context *ctx, bool on);
void yecc_context_set_trace_sema(struct yecc_context *ctx, bool on);
void yecc_context_set_trace_ir(struct yecc_context *ctx, bool on);
void yecc_context_set_trace_codegen(struct yecc_context *ctx, bool on);

void yecc_warning_enable(struct yecc_context *ctx, enum yecc_warning w, bool on);
void yecc_warning_as_error(struct yecc_context *ctx, enum yecc_warning w, bool on);

const char *yecc_lang_standard_name(enum yecc_lang_standard std);
const char *yecc_opt_level_name(enum yecc_opt_level lvl);
const char *yecc_reloc_model_name(enum yecc_reloc_model rm);
const char *yecc_code_model_name(enum yecc_code_model cm);
const char *yecc_float_mode_name(enum yecc_float_mode fm);

#endif /* CONTEXT_H */
