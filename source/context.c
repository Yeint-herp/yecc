#include <context.h>
#include <string.h>
#include <utils.h>
#include <vector.h>

void yecc_context_init(struct yecc_context *ctx) {
	if (!ctx)
		return;

	memset(ctx, 0, sizeof *ctx);

	ctx->lang_std = YECC_LANG_C23;
	ctx->opt_level = YECC_O0;
	ctx->target_stage = YECC_TARGET_STAGE_EXE;

	ctx->target_triple = nullptr;
	ctx->sysroot = nullptr;

	ctx->warning_enabled_mask = 0;
	ctx->warning_error_mask = 0;

	ctx->color_mode = YECC_COLOR_AUTO;
	ctx->unknown_pragma_policy = YECC_PRAGMA_WARN;
	ctx->warnings_as_errors = false;
	ctx->pedantic = false;
	ctx->max_errors = 20;
	ctx->gnu_extensions = true;
	ctx->yecc_extensions = true;
	ctx->no_short_enums = false;

	ctx->enable_trigraphs = false;

	ctx->float_mode = YECC_FLOAT_FULL;
	ctx->fast_math = false;
	ctx->strict_ieee = true;
	ctx->reloc_model = YECC_RELOC_PIE;
	ctx->code_model = YECC_CODE_SMALL;

	ctx->cpu = nullptr;
	ctx->tune = nullptr;
	ctx->cpu_features_enable_mask = 0ull;
	ctx->cpu_features_disable_mask = 0ull;

	ctx->use_standard_includes = true;
	ctx->nostdlib = false;
	ctx->nodefaultlibs = false;
	ctx->nostartfiles = false;
	ctx->static_link = false;
	ctx->link_libc = true;
	ctx->link_libm = false;
	ctx->link_libcompilerrt = false;
	ctx->output_path = nullptr;

	ctx->trace_lexer = false;
	ctx->trace_pp = false;
	ctx->trace_parser = false;
	ctx->trace_sema = false;
	ctx->trace_ir = false;
	ctx->trace_codegen = false;
}

void yecc_context_destroy(struct yecc_context *ctx) {
	if (!ctx)
		return;

	vector_destroy(&ctx->include_paths);
	vector_destroy(&ctx->system_include_paths);
	vector_destroy(&ctx->predefined_macros);

	memset(ctx, 0, sizeof *ctx);
}

void yecc_context_set_lang_standard(struct yecc_context *ctx, enum yecc_lang_standard std) {
	if (ctx)
		ctx->lang_std = std;
}
void yecc_context_set_opt_level(struct yecc_context *ctx, enum yecc_opt_level lvl) {
	if (ctx)
		ctx->opt_level = lvl;
}
void yecc_context_set_target_stage(struct yecc_context *ctx, enum yecc_target_stage stg) {
	if (ctx)
		ctx->target_stage = stg;
}

void yecc_context_set_target_triple(struct yecc_context *ctx, const char *triple) {
	if (ctx)
		ctx->target_triple = triple;
}
void yecc_context_set_sysroot(struct yecc_context *ctx, const char *sysroot) {
	if (ctx)
		ctx->sysroot = sysroot;
}

void yecc_context_set_color_mode(struct yecc_context *ctx, enum yecc_color_mode mode) {
	if (ctx)
		ctx->color_mode = mode;
}
void yecc_context_set_warnings_as_errors(struct yecc_context *ctx, bool on) {
	if (ctx)
		ctx->warnings_as_errors = on;
}
void yecc_context_set_pedantic(struct yecc_context *ctx, bool on) {
	if (ctx) {
		ctx->pedantic = on;
		yecc_warning_enable(ctx, YECC_W_PEDANTIC, true);
	}
}

void yecc_context_set_max_errors(struct yecc_context *ctx, int n) {
	if (ctx)
		ctx->max_errors = n;
}
void yecc_context_set_unknown_pragma_policy(struct yecc_context *ctx, enum yecc_pragma_policy pol) {
	if (ctx)
		ctx->unknown_pragma_policy = pol;
}

void yecc_context_set_gnu_extensions(struct yecc_context *ctx, bool on) {
	if (ctx)
		ctx->gnu_extensions = on;
}
void yecc_context_set_yecc_extensions(struct yecc_context *ctx, bool on) {
	if (ctx) {
		ctx->yecc_extensions = on;
		if (on)
			ctx->gnu_extensions = on;
	}
}
void yecc_context_set_no_short_enums(struct yecc_context *ctx, bool on) {
	if (ctx)
		ctx->no_short_enums = on;
}

void yecc_context_set_enable_trigraphs(struct yecc_context *ctx, bool on) {
	if (ctx)
		ctx->enable_trigraphs = on;
}

void yecc_context_set_float_mode(struct yecc_context *ctx, enum yecc_float_mode fm) {
	if (ctx)
		ctx->float_mode = fm;
}
void yecc_context_set_fast_math(struct yecc_context *ctx, bool on) {
	if (ctx)
		ctx->fast_math = on;
}
void yecc_context_set_strict_ieee(struct yecc_context *ctx, bool on) {
	if (ctx)
		ctx->strict_ieee = on;
}
void yecc_context_set_reloc_model(struct yecc_context *ctx, enum yecc_reloc_model rm) {
	if (ctx)
		ctx->reloc_model = rm;
}
void yecc_context_set_code_model(struct yecc_context *ctx, enum yecc_code_model cm) {
	if (ctx)
		ctx->code_model = cm;
}

void yecc_context_set_cpu(struct yecc_context *ctx, const char *cpu) {
	if (ctx)
		ctx->cpu = cpu;
}
void yecc_context_set_tune(struct yecc_context *ctx, const char *tune) {
	if (ctx)
		ctx->tune = tune;
}

void yecc_context_enable_feature(struct yecc_context *ctx, enum yecc_cpu_feature f, bool on) {
	if (!ctx)
		return;
	set_bit_u64(&ctx->cpu_features_enable_mask, (unsigned)f, on);
}
void yecc_context_disable_feature(struct yecc_context *ctx, enum yecc_cpu_feature f, bool on) {
	if (!ctx)
		return;
	set_bit_u64(&ctx->cpu_features_disable_mask, (unsigned)f, on);
}

void yecc_context_set_use_standard_includes(struct yecc_context *ctx, bool on) {
	if (ctx)
		ctx->use_standard_includes = on;
}
void yecc_context_set_nostdlib(struct yecc_context *ctx, bool on) {
	if (ctx)
		ctx->nostdlib = on;
}
void yecc_context_set_nodefaultlibs(struct yecc_context *ctx, bool on) {
	if (ctx)
		ctx->nodefaultlibs = on;
}
void yecc_context_set_nostartfiles(struct yecc_context *ctx, bool on) {
	if (ctx)
		ctx->nostartfiles = on;
}
void yecc_context_set_static_link(struct yecc_context *ctx, bool on) {
	if (ctx)
		ctx->static_link = on;
}
void yecc_context_set_output_path(struct yecc_context *ctx, const char *out) {
	if (ctx)
		ctx->output_path = out;
}

void yecc_context_set_link_libc(struct yecc_context *ctx, bool on) {
	if (ctx)
		ctx->link_libc = on;
}
void yecc_context_set_link_libm(struct yecc_context *ctx, bool on) {
	if (ctx)
		ctx->link_libm = on;
}
void yecc_context_set_link_compilerrt(struct yecc_context *ctx, bool on) {
	if (ctx)
		ctx->link_libcompilerrt = on;
}

/* Paths/macros are borrowed; we just push the pointers. */
void yecc_context_add_include_path(struct yecc_context *ctx, const char *path, bool system_path) {
	if (!ctx || !path)
		return;
	if (system_path)
		vector_push(&ctx->system_include_paths, path);
	else
		vector_push(&ctx->include_paths, path);
}

void yecc_context_add_define(struct yecc_context *ctx, const char *macro_eq_value) {
	if (!ctx || !macro_eq_value)
		return;
	vector_push(&ctx->predefined_macros, macro_eq_value);
}

void yecc_context_set_trace_lexer(struct yecc_context *ctx, bool on) {
	if (ctx)
		ctx->trace_lexer = on;
}
void yecc_context_set_trace_pp(struct yecc_context *ctx, bool on) {
	if (ctx)
		ctx->trace_pp = on;
}
void yecc_context_set_trace_parser(struct yecc_context *ctx, bool on) {
	if (ctx)
		ctx->trace_parser = on;
}
void yecc_context_set_trace_sema(struct yecc_context *ctx, bool on) {
	if (ctx)
		ctx->trace_sema = on;
}
void yecc_context_set_trace_ir(struct yecc_context *ctx, bool on) {
	if (ctx)
		ctx->trace_ir = on;
}
void yecc_context_set_trace_codegen(struct yecc_context *ctx, bool on) {
	if (ctx)
		ctx->trace_codegen = on;
}

void yecc_warning_enable(struct yecc_context *ctx, enum yecc_warning w, bool on) {
	if (!ctx)
		return;
	set_bit_u32(&ctx->warning_enabled_mask, (unsigned)w, on);
}
void yecc_warning_as_error(struct yecc_context *ctx, enum yecc_warning w, bool on) {
	if (!ctx)
		return;
	set_bit_u32(&ctx->warning_error_mask, (unsigned)w, on);
}

const char *yecc_lang_standard_name(enum yecc_lang_standard std) {
	switch (std) {
	case YECC_LANG_C89:
		return "c89";
	case YECC_LANG_C99:
		return "c99";
	case YECC_LANG_C11:
		return "c11";
	case YECC_LANG_C17:
		return "c17";
	case YECC_LANG_C23:
		return "c23";
	default:
		return "unknown";
	}
}

const char *yecc_opt_level_name(enum yecc_opt_level lvl) {
	switch (lvl) {
	case YECC_O0:
		return "O0";
	case YECC_O1:
		return "O1";
	case YECC_O2:
		return "O2";
	case YECC_O3:
		return "O3";
	default:
		return "On/a";
	}
}

const char *yecc_reloc_model_name(enum yecc_reloc_model rm) {
	switch (rm) {
	case YECC_RELOC_STATIC:
		return "static";
	case YECC_RELOC_PIC:
		return "pic";
	case YECC_RELOC_PIE:
		return "pie";
	default:
		return "n/a";
	}
}

const char *yecc_code_model_name(enum yecc_code_model cm) {
	switch (cm) {
	case YECC_CODE_SMALL:
		return "small";
	case YECC_CODE_MEDIUM:
		return "medium";
	case YECC_CODE_LARGE:
		return "large";
	default:
		return "n/a";
	}
}

const char *yecc_float_mode_name(enum yecc_float_mode fm) {
	switch (fm) {
	case YECC_FLOAT_FULL:
		return "full";
	case YECC_FLOAT_SOFT:
		return "soft";
	case YECC_FLOAT_DISABLED:
		return "disabled";
	default:
		return "n/a";
	}
}
