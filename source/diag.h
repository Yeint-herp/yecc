#ifndef DIAG_H
#define DIAG_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <streamer.h>

/**
 * diag.h
 *
 * A simple diagnostics module for reporting errors, warnings, notes, and info
 * messages with optional ANSI color highlighting and source‐location context.
 */

/** Different levels of diagnostics. */
typedef enum { DIAG_LEVEL_ERROR, DIAG_LEVEL_WARNING, DIAG_LEVEL_NOTE, DIAG_LEVEL_INFO } diag_level;

/**
 * Initialize the diagnostics subsystem.
 *
 * Detects whether stderr is connected to a terminal and reads NO_COLOR /
 * CLICOLOR_FORCE environment variables to decide whether to emit ANSI colors.
 * Must be called before any diagnostics are emitted; it's idempotent.
 */
void diag_init(void);

/* report an error (non‐fatal) */
void diag_error(struct source_span span, const char *fmt, ...);

void diag_warning(struct source_span span, const char *fmt, ...);
void diag_note(struct source_span span, const char *fmt, ...);
void diag_info(struct source_span span, const char *fmt, ...);

void diag_context(diag_level level, struct source_span span, const char *fmt, ...);

#endif /* DIAG_H */
