// REPL action processing - history navigation
#include "repl_actions_internal.h"
#include "repl.h"
#include "shared.h"
#include "panic.h"
#include "history.h"
#include "completion.h"
#include "input_buffer/core.h"
#include <assert.h>
#include <talloc.h>
#include <string.h>

/**
 * @brief Load history entry into input buffer
 *
 * Helper function to replace input buffer contents with a history entry
 * and reset cursor to position 0.
 *
 * @param repl REPL context
 * @param entry History entry text
 * @return res_t Result
 */
static res_t load_history_entry_(ik_repl_ctx_t *repl, const char *entry)
{
    assert(repl != NULL); /* LCOV_EXCL_BR_LINE */
    assert(entry != NULL); /* LCOV_EXCL_BR_LINE */

    size_t entry_len = strlen(entry);
    return ik_input_buffer_set_text(repl->input_buffer, entry, entry_len);
}

/**
 * @brief Handle arrow up action - viewport scroll, completion navigation, history navigation, or cursor up
 *
 * When viewport_offset > 0, scrolls the viewport up instead of navigating history.
 * This allows scroll wheel (which sends arrow sequences in alternate scroll mode)
 * to scroll the viewport naturally.
 *
 * @param repl REPL context
 * @return res_t Result
 */
res_t ik_repl_handle_arrow_up_action(ik_repl_ctx_t *repl)
{
    assert(repl != NULL); /* LCOV_EXCL_BR_LINE */
    assert(repl->input_buffer != NULL); /* LCOV_EXCL_BR_LINE */

    // If viewport is scrolled, scroll up instead of navigating history
    if (repl->viewport_offset > 0) {
        return ik_repl_handle_scroll_up_action(repl);
    }

    // If completion is active, navigate to previous candidate
    if (repl->completion != NULL) {
        ik_completion_prev(repl->completion);
        return OK(NULL);
    }

    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res_t res = ik_input_buffer_get_cursor_position(repl->input_buffer, &byte_offset, &grapheme_offset);
    if (is_err(&res)) return res;  // LCOV_EXCL_LINE - allocation failure in cursor operations

    if (byte_offset != 0) {
        return ik_input_buffer_cursor_up(repl->input_buffer);
    }

    if (repl->shared->history == NULL) {  // LCOV_EXCL_BR_LINE
        return OK(NULL);  // LCOV_EXCL_LINE
    }

    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(repl->input_buffer, &text_len);

    const char *entry = NULL;
    if (!ik_history_is_browsing(repl->shared->history)) {
        char *pending = talloc_zero_size(repl, text_len + 1);
        if (pending == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        if (text_len > 0) {
            memcpy(pending, text, text_len);
        }
        pending[text_len] = '\0';

        res = ik_history_start_browsing(repl->shared->history, pending);
        if (is_err(&res)) {  // LCOV_EXCL_BR_LINE - OOM in history_start_browsing
            talloc_free(pending);  // LCOV_EXCL_LINE
            return res;  // LCOV_EXCL_LINE
        }
        talloc_free(pending);

        entry = ik_history_get_current(repl->shared->history);
    } else {
        entry = ik_history_prev(repl->shared->history);
    }

    if (entry == NULL) {
        return OK(NULL);
    }

    return load_history_entry_(repl, entry);
}

/**
 * @brief Handle arrow down action - viewport scroll, completion navigation, history navigation, or cursor down
 *
 * When viewport_offset > 0, scrolls the viewport down instead of navigating history.
 * This allows scroll wheel (which sends arrow sequences in alternate scroll mode)
 * to scroll the viewport naturally.
 *
 * @param repl REPL context
 * @return res_t Result
 */
res_t ik_repl_handle_arrow_down_action(ik_repl_ctx_t *repl)
{
    assert(repl != NULL); /* LCOV_EXCL_BR_LINE */
    assert(repl->input_buffer != NULL); /* LCOV_EXCL_BR_LINE */

    // If viewport is scrolled, scroll down instead of navigating history
    if (repl->viewport_offset > 0) {
        return ik_repl_handle_scroll_down_action(repl);
    }

    // If completion is active, navigate to next candidate
    if (repl->completion != NULL) {
        ik_completion_next(repl->completion);
        return OK(NULL);
    }

    size_t byte_offset = 0;
    size_t grapheme_offset = 0;
    res_t res = ik_input_buffer_get_cursor_position(repl->input_buffer, &byte_offset, &grapheme_offset);
    if (is_err(&res)) return res;  // LCOV_EXCL_LINE - allocation failure in cursor operations

    if (byte_offset != 0) {  // LCOV_EXCL_BR_LINE
        return ik_input_buffer_cursor_down(repl->input_buffer);  // LCOV_EXCL_LINE
    }

    if (repl->shared->history != NULL && ik_history_is_browsing(repl->shared->history)) {
        const char *entry = ik_history_next(repl->shared->history);
        if (entry == NULL) {  // LCOV_EXCL_BR_LINE
            return OK(NULL);  // LCOV_EXCL_LINE
        }
        return load_history_entry_(repl, entry);
    }

    return ik_input_buffer_cursor_down(repl->input_buffer);
}

/**
 * @brief Handle Ctrl+P - history previous (rel-05)
 *
 * Navigate to previous entry in history. This is the dedicated history
 * navigation key, separate from arrow up which now handles cursor movement.
 *
 * @param repl REPL context
 * @return res_t Result
 */
res_t ik_repl_handle_history_prev_action(ik_repl_ctx_t *repl)
{
    assert(repl != NULL); /* LCOV_EXCL_BR_LINE */

    if (repl->shared->history == NULL) {  // LCOV_EXCL_BR_LINE
        return OK(NULL);  // LCOV_EXCL_LINE
    }

    size_t text_len = 0;
    const char *text = ik_input_buffer_get_text(repl->input_buffer, &text_len);

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

/**
 * @brief Handle Ctrl+N - history next (rel-05)
 *
 * Navigate to next entry in history. This is the dedicated history
 * navigation key, separate from arrow down which now handles cursor movement.
 *
 * @param repl REPL context
 * @return res_t Result
 */
res_t ik_repl_handle_history_next_action(ik_repl_ctx_t *repl)
{
    assert(repl != NULL); /* LCOV_EXCL_BR_LINE */

    if (repl->shared->history == NULL || !ik_history_is_browsing(repl->shared->history)) {
        return OK(NULL);
    }

    const char *entry = ik_history_next(repl->shared->history);
    if (entry == NULL) {  // LCOV_EXCL_BR_LINE
        return OK(NULL);  // LCOV_EXCL_LINE
    }

    return load_history_entry_(repl, entry);
}
