#ifndef IK_PANIC_H
#define IK_PANIC_H

#include "terminal.h"

/**
 * Global terminal context for panic handler.
 * Set this after successful terminal initialization to enable
 * terminal restoration on panic.
 */
extern ik_term_ctx_t *g_term_ctx_for_panic;

/**
 * Async-signal-safe panic handler.
 *
 * This function:
 * 1. Restores terminal state if g_term_ctx_for_panic is set
 * 2. Writes error message to stderr using only write()
 * 3. Calls abort()
 *
 * This function never returns and never allocates memory.
 * It is async-signal-safe and can be called from signal handlers.
 *
 * @param msg Error message (should be string literal)
 * @param file Source file name (typically __FILE__)
 * @param line Source line number (typically __LINE__)
 */
__attribute__((noreturn))
void ik_panic_impl(const char *msg, const char *file, int line);

/**
 * Talloc abort handler.
 * Called when talloc detects internal errors.
 */
__attribute__((noreturn))
void ik_talloc_abort_handler(const char *reason);

/**
 * PANIC macro for unrecoverable errors.
 *
 * Use this for:
 * - Out of memory conditions
 * - Logic errors indicating corruption or impossible states
 *
 * Should be used sparingly (~1-2 per 1000 LOC).
 *
 * Example:
 *   ptr = ik_talloc_zero_wrapper(ctx, size);
 *   if (ptr == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
 */
#define PANIC(msg) ik_panic_impl((msg), __FILE__, __LINE__)

#endif // IK_PANIC_H
