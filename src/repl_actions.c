// REPL action processing module
#include "repl_actions.h"
#include "repl_actions_internal.h"
#include "repl.h"
#include "shared.h"
#include "history.h"
#include "scrollback.h"
#include "input_buffer/core.h"
#include <assert.h>
#include <time.h>

/**
 * @brief Append multi-line output to scrollback (splits by newlines)
 *
 * Exposed for testing edge cases.
 *
 * @param scrollback Scrollback buffer
 * @param output Multi-line text (may contain \n)
 * @param output_len Length of output
 */
void ik_repl_append_multiline_to_scrollback(ik_scrollback_t *scrollback, const char *output, size_t output_len)
{
    assert(scrollback != NULL);  /* LCOV_EXCL_BR_LINE */
    assert(output != NULL);  /* LCOV_EXCL_BR_LINE */

    if (output_len == 0) {
        return;
    }

    // Split output by newlines and append each line separately
    size_t line_start = 0;
    for (size_t i = 0; i <= output_len; i++) {
        if (i == output_len || output[i] == '\n') {
            // Found end of line or end of string
            size_t line_len = i - line_start;
            if (line_len > 0 || i < output_len) {
                ik_scrollback_append_line(scrollback, output + line_start, line_len);
            }
            line_start = i + 1;  // Start of next line (skip the \n)
        }
    }
}

res_t ik_repl_process_action(ik_repl_ctx_t *repl, const ik_input_action_t *action)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(action != NULL);   /* LCOV_EXCL_BR_LINE */

    // Intercept arrow up/down events for burst detection (rel-05)
    if ((action->type == IK_INPUT_ARROW_UP || action->type == IK_INPUT_ARROW_DOWN) &&
        repl->arrow_detector != NULL) {
        // Get current time for burst detection
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        int64_t now_ms = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

        // Process through arrow burst detector
        ik_arrow_burst_result_t result = ik_arrow_burst_process(
            repl->arrow_detector, action->type, now_ms);

        // Route based on burst detection result
        switch (result) {  // LCOV_EXCL_BR_LINE
            case IK_ARROW_BURST_RESULT_SCROLL_UP:
                return ik_repl_handle_scroll_up_action(repl);
            case IK_ARROW_BURST_RESULT_SCROLL_DOWN:
                return ik_repl_handle_scroll_down_action(repl);
            case IK_ARROW_BURST_RESULT_CURSOR_UP:
                ik_repl_dismiss_completion(repl);
                repl->viewport_offset = 0;
                return ik_input_buffer_cursor_up(repl->input_buffer);
            case IK_ARROW_BURST_RESULT_CURSOR_DOWN:
                ik_repl_dismiss_completion(repl);
                repl->viewport_offset = 0;
                return ik_input_buffer_cursor_down(repl->input_buffer);
            case IK_ARROW_BURST_RESULT_NONE:
                // Still buffering, don't process yet
                return OK(NULL);
        }
    }

    switch (action->type) { // LCOV_EXCL_BR_LINE
        case IK_INPUT_CHAR: {
            // Handle Space when completion is active - commit selection and dismiss
            if (action->codepoint == ' ' && repl->completion != NULL) {
                return ik_repl_handle_completion_space_commit(repl);
            }

            if (repl->shared->history != NULL && ik_history_is_browsing(repl->shared->history)) {  // LCOV_EXCL_BR_LINE
                ik_history_stop_browsing(repl->shared->history);  // LCOV_EXCL_LINE
            }
            repl->viewport_offset = 0;

            // Insert the character first
            res_t res = ik_input_buffer_insert_codepoint(repl->input_buffer, action->codepoint);
            if (is_err(&res)) {
                return res;
            }

            // Update completion if active
            ik_repl_update_completion_after_char(repl);

            return OK(NULL);
        }
        case IK_INPUT_INSERT_NEWLINE:
            repl->viewport_offset = 0;
            return ik_input_buffer_insert_newline(repl->input_buffer);
        case IK_INPUT_NEWLINE:
            return ik_repl_handle_newline_action(repl);
        case IK_INPUT_BACKSPACE: {
            repl->viewport_offset = 0;
            res_t res = ik_input_buffer_backspace(repl->input_buffer);
            if (is_err(&res)) {  // LCOV_EXCL_BR_LINE
                return res;  // LCOV_EXCL_LINE
            }

            // Update completion if active
            ik_repl_update_completion_after_char(repl);

            return OK(NULL);
        }
        case IK_INPUT_DELETE:
            repl->viewport_offset = 0;
            return ik_input_buffer_delete(repl->input_buffer);
        case IK_INPUT_ARROW_LEFT:
            ik_repl_dismiss_completion(repl);
            repl->viewport_offset = 0;
            return ik_input_buffer_cursor_left(repl->input_buffer);
        case IK_INPUT_ARROW_RIGHT:
            ik_repl_dismiss_completion(repl);
            repl->viewport_offset = 0;
            return ik_input_buffer_cursor_right(repl->input_buffer);
        case IK_INPUT_ARROW_UP:
            return ik_repl_handle_arrow_up_action(repl);
        case IK_INPUT_ARROW_DOWN:
            return ik_repl_handle_arrow_down_action(repl);
        case IK_INPUT_PAGE_UP:
            return ik_repl_handle_page_up_action(repl);
        case IK_INPUT_PAGE_DOWN:
            return ik_repl_handle_page_down_action(repl);
        case IK_INPUT_SCROLL_UP:
            return ik_repl_handle_scroll_up_action(repl);
        case IK_INPUT_SCROLL_DOWN:
            return ik_repl_handle_scroll_down_action(repl);
        case IK_INPUT_CTRL_A:
            repl->viewport_offset = 0;
            return ik_input_buffer_cursor_to_line_start(repl->input_buffer);
        case IK_INPUT_CTRL_E:
            repl->viewport_offset = 0;
            return ik_input_buffer_cursor_to_line_end(repl->input_buffer);
        case IK_INPUT_CTRL_K:
            repl->viewport_offset = 0;
            return ik_input_buffer_kill_to_line_end(repl->input_buffer);
        case IK_INPUT_CTRL_N:
            return ik_repl_handle_history_next_action(repl);
        case IK_INPUT_CTRL_P:
            return ik_repl_handle_history_prev_action(repl);
        case IK_INPUT_CTRL_U:
            repl->viewport_offset = 0;
            return ik_input_buffer_kill_line(repl->input_buffer);
        case IK_INPUT_CTRL_W:
            repl->viewport_offset = 0;
            return ik_input_buffer_delete_word_backward(repl->input_buffer);
        case IK_INPUT_CTRL_C:
            repl->quit = true;
            return OK(NULL);
        case IK_INPUT_TAB:
            return ik_repl_handle_tab_action(repl);
        case IK_INPUT_ESCAPE: {
            // If completion is active, revert to original input before ESC dismisses
            if (repl->completion != NULL && repl->completion->original_input != NULL) {
                // Revert to original input
                const char *original = repl->completion->original_input;
                res_t res = ik_input_buffer_set_text(repl->input_buffer,
                                                      original, strlen(original));
                if (is_err(&res)) {  // LCOV_EXCL_BR_LINE
                    return res;  // LCOV_EXCL_LINE
                }
            }
            ik_repl_dismiss_completion(repl);
            return OK(NULL);
        }
        case IK_INPUT_UNKNOWN:
            return OK(NULL);
        default: // LCOV_EXCL_LINE
            PANIC("Invalid input action type"); // LCOV_EXCL_LINE
    }
}
