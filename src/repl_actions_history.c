// REPL action processing - history navigation
#include "repl_actions_internal.h"
#include "repl.h"
#include "agent.h"
#include "shared.h"
#include "panic.h"
#include "history.h"
#include "completion.h"
#include "input_buffer/core.h"
#include <assert.h>
#include <talloc.h>
#include <string.h>


#include "poison.h"
/**
 * @brief Load history entry into input buffer
 *
 * Helper function to replace input buffer contents with a history entry
 * and reset cursor to position 0.
 *
 * Reserved for future Ctrl+R reverse search implementation.
 *
 * @param repl REPL context
 * @param entry History entry text
 * @return res_t Result
 */
// LCOV_EXCL_START - Reserved for future Ctrl+R implementation
static res_t load_history_entry_(ik_repl_ctx_t *repl, const char *entry)
{
    assert(repl != NULL); /* LCOV_EXCL_BR_LINE */
    assert(entry != NULL); /* LCOV_EXCL_BR_LINE */

    size_t entry_len = strlen(entry);
    return ik_input_buffer_set_text(repl->current->input_buffer, entry, entry_len);
}
// LCOV_EXCL_STOP

/**
 * @brief Handle arrow up action - completion navigation or cursor up
 *
 * Arrow keys ONLY move cursor. History navigation is disabled (will use Ctrl+R).
 *
 * @param repl REPL context
 * @return res_t Result
 */
res_t ik_repl_handle_arrow_up_action(ik_repl_ctx_t *repl)
{
    assert(repl != NULL); /* LCOV_EXCL_BR_LINE */
    assert(repl->current->input_buffer != NULL); /* LCOV_EXCL_BR_LINE */

    // If viewport is scrolled, scroll up instead of moving cursor
    if (repl->current->viewport_offset > 0) {
        return ik_repl_handle_scroll_up_action(repl);
    }

    // If completion is active, navigate to previous candidate
    if (repl->current->completion != NULL) {
        ik_completion_prev(repl->current->completion);
        return OK(NULL);
    }

    // Always move cursor up, never browse history
    return ik_input_buffer_cursor_up(repl->current->input_buffer);
}

/**
 * @brief Handle arrow down action - completion navigation or cursor down
 *
 * Arrow keys ONLY move cursor. History navigation is disabled (will use Ctrl+R).
 *
 * @param repl REPL context
 * @return res_t Result
 */
res_t ik_repl_handle_arrow_down_action(ik_repl_ctx_t *repl)
{
    assert(repl != NULL); /* LCOV_EXCL_BR_LINE */
    assert(repl->current->input_buffer != NULL); /* LCOV_EXCL_BR_LINE */

    // If viewport is scrolled, scroll down instead of moving cursor
    if (repl->current->viewport_offset > 0) {
        return ik_repl_handle_scroll_down_action(repl);
    }

    // If completion is active, navigate to next candidate
    if (repl->current->completion != NULL) {
        ik_completion_next(repl->current->completion);
        return OK(NULL);
    }

    // Always move cursor down, never browse history
    return ik_input_buffer_cursor_down(repl->current->input_buffer);
}

/**
 * @brief Handle Ctrl+P - history previous (currently disabled)
 *
 * Reserved for future Ctrl+R reverse search implementation.
 * Currently a no-op (handled in repl_actions.c).
 *
 * @param repl REPL context
 * @return res_t Result
 */
// LCOV_EXCL_START - Reserved for future Ctrl+R implementation
res_t ik_repl_handle_history_prev_action(ik_repl_ctx_t *repl)
{
    assert(repl != NULL); /* LCOV_EXCL_BR_LINE */

    if (repl->shared->history == NULL) {  // LCOV_EXCL_BR_LINE
        return OK(NULL);  // LCOV_EXCL_LINE
    }

    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(repl->current->input_buffer, &text_len);

    const char *entry = NULL;
    if (!ik_history_is_browsing(repl->shared->history)) {
        // Start browsing with current input as pending
        char *pending = talloc_zero_size(repl, text_len + 1);
        if (pending == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        if (text_len > 0) {
            memcpy(pending, text, text_len);
        }
        pending[text_len] = '\0';

        res_t res = ik_history_start_browsing(repl->shared->history, pending);
        if (is_err(&res)) {  // LCOV_EXCL_BR_LINE - OOM in history_start_browsing
            talloc_free(pending);  // LCOV_EXCL_LINE
            return res;  // LCOV_EXCL_LINE
        }
        talloc_free(pending);

        entry = ik_history_get_current(repl->shared->history);
    } else {
        // Already browsing, move to previous
        entry = ik_history_prev(repl->shared->history);
    }

    if (entry == NULL) {
        return OK(NULL);
    }

    return load_history_entry_(repl, entry);
}
// LCOV_EXCL_STOP

/**
 * @brief Handle Ctrl+N - history next (currently disabled)
 *
 * Reserved for future Ctrl+R reverse search implementation.
 * Currently a no-op (handled in repl_actions.c).
 *
 * @param repl REPL context
 * @return res_t Result
 */
// LCOV_EXCL_START - Reserved for future Ctrl+R implementation
res_t ik_repl_handle_history_next_action(ik_repl_ctx_t *repl)
{
    assert(repl != NULL); /* LCOV_EXCL_BR_LINE */

    if (repl->shared->history == NULL || !ik_history_is_browsing(repl->shared->history)) {  // LCOV_EXCL_BR_LINE - Defensive: history NULL if disabled
        return OK(NULL);
    }

    const char *entry = ik_history_next(repl->shared->history);
    if (entry == NULL) {  // LCOV_EXCL_BR_LINE
        return OK(NULL);  // LCOV_EXCL_LINE
    }

    return load_history_entry_(repl, entry);
}
// LCOV_EXCL_STOP
