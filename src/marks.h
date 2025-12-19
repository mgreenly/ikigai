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
 * TODO(DI): This function violates DI principles by taking repl context instead of explicit dependencies.
 * It reaches into repl->current->conversation, repl->current->marks, and repl->current->scrollback.
 * Should be refactored to: ik_mark_create(TALLOC_CTX *ctx, ik_openai_conversation_t *conv,
 *                                         ik_mark_t ***marks_ptr, size_t *mark_count_ptr,
 *                                         ik_scrollback_t *scrollback, const char *label, ik_mark_t **out)
 * See tasks/init-mark-decouple.md for full refactoring plan.
 *
 * @param repl   REPL context
 * @param label  Optional label (or NULL for unlabeled mark)
 * @return       OK(NULL) or ERR(...)
 */
res_t ik_mark_create(ik_repl_ctx_t *repl, const char *label);

/**
 * Find a mark by label or get the most recent mark
 *
 * TODO(DI): This function violates DI principles by taking repl context instead of explicit dependencies.
 * It reaches into repl->current->marks and repl->current->mark_count.
 * Should be refactored to: ik_mark_find(ik_mark_t **marks, size_t mark_count, const char *label, ik_mark_t **out)
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
 * TODO(DI): This function violates DI principles by taking repl context instead of explicit dependencies.
 * It reaches into repl->current->conversation, repl->current->marks, repl->current->scrollback, and repl->shared->cfg.
 * Should be refactored to take explicit parameters for conversation, marks array, scrollback, and config.
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
 * TODO(DI): This function violates DI principles by taking repl context.
 * It delegates to ik_mark_find and ik_mark_rewind_to_mark, both of which have DI violations.
 *
 * @param repl   REPL context
 * @param label  Label to rewind to (or NULL for most recent)
 * @return       OK(NULL) or ERR(...)
 */
res_t ik_mark_rewind_to(ik_repl_ctx_t *repl, const char *label);
