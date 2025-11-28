#pragma once

#include "error.h"
#include "repl.h"

/**
 * Mark management module
 *
 * Provides checkpoint/rollback functionality for conversations.
 * Marks allow users to save conversation state and rewind to previous points.
 */

/**
 * Create a mark at the current conversation position
 *
 * @param repl   REPL context
 * @param label  Optional label (or NULL for unlabeled mark)
 * @return       OK(NULL) or ERR(...)
 */
res_t ik_mark_create(ik_repl_ctx_t *repl, const char *label);

/**
 * Find a mark by label or get the most recent mark
 *
 * @param repl      REPL context
 * @param label     Label to find (or NULL for most recent)
 * @param mark_out  Output pointer for found mark
 * @return          OK(mark) or ERR(...)
 */
res_t ik_mark_find(ik_repl_ctx_t *repl, const char *label, ik_mark_t **mark_out);

/**
 * Rewind conversation to a specific mark
 *
 * Truncates conversation to the mark position and rebuilds scrollback.
 * Removes all marks after the target mark, but keeps the target mark itself.
 * This allows the mark to be reused for subsequent rewinds.
 *
 * @param repl        REPL context
 * @param target_mark Mark to rewind to
 * @return            OK(NULL) or ERR(...)
 */
res_t ik_mark_rewind_to_mark(ik_repl_ctx_t *repl, ik_mark_t *target_mark);

/**
 * Rewind conversation to a mark by label
 *
 * Finds the mark by label (or most recent if NULL), then rewinds to it.
 * Truncates conversation to the mark position and rebuilds scrollback.
 * Removes all marks after the target mark, but keeps the target mark itself.
 *
 * @param repl   REPL context
 * @param label  Label to rewind to (or NULL for most recent)
 * @return       OK(NULL) or ERR(...)
 */
res_t ik_mark_rewind_to(ik_repl_ctx_t *repl, const char *label);
