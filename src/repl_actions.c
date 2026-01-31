// REPL action processing module
#include "repl_actions.h"
#include "repl_actions_internal.h"

#include "history.h"
#include "input_buffer/core.h"
#include "logger.h"
#include "panic.h"
#include "providers/provider.h"
#include "repl.h"
#include "agent.h"
#include "scrollback.h"
#include "shared.h"
#include "wrapper.h"

#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

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

/**
 * @brief Process arrow up/down through scroll detector
 *
 * Intercepts arrow keys to distinguish between keyboard navigation and
 * mouse scroll wheel events.
 *
 * @param repl REPL context
 * @param action Input action (must be ARROW_UP or ARROW_DOWN)
 * @return OK with NULL if handled, or propagates error
 */
res_t ik_repl_process_scroll_detection(ik_repl_ctx_t *repl, const ik_input_action_t *action)
{
    assert(repl != NULL);  /* LCOV_EXCL_BR_LINE */
    assert(action != NULL);  /* LCOV_EXCL_BR_LINE */
    assert(action->type == IK_INPUT_ARROW_UP || action->type == IK_INPUT_ARROW_DOWN);  /* LCOV_EXCL_BR_LINE */

    if (repl->scroll_det == NULL) {
        return OK(NULL);  // No scroll detector - caller should handle as normal arrow
    }

    // Get current time for scroll detection
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    int64_t now_ms = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;

    // Process through scroll detector
    ik_scroll_result_t result = ik_scroll_detector_process_arrow(
        repl->scroll_det, action->type, now_ms);

    // Route based on scroll detection result
    // LCOV_EXCL_START - Scroll detection requires precise timing control
    switch (result) {
        case IK_SCROLL_RESULT_SCROLL_UP:
            return ik_repl_handle_scroll_up_action(repl);
        case IK_SCROLL_RESULT_SCROLL_DOWN:
            return ik_repl_handle_scroll_down_action(repl);
        case IK_SCROLL_RESULT_ARROW_UP:
        case IK_SCROLL_RESULT_ARROW_DOWN:
            // Fall through to handle as normal arrow
            return OK(NULL);
        case IK_SCROLL_RESULT_NONE:
            // Buffered, waiting for more - signal handled
            return OK((void *)1);
        case IK_SCROLL_RESULT_ABSORBED:
            // Arrow absorbed as part of burst - signal handled
            return OK((void *)1);
    }
    // LCOV_EXCL_STOP

    PANIC("Invalid scroll result");  // LCOV_EXCL_LINE
}

/**
 * @brief Flush pending arrow from scroll detector
 *
 * Called when non-arrow events occur to flush any buffered arrow key.
 *
 * @param repl REPL context
 * @param action Current input action (must not be ARROW_UP/DOWN/UNKNOWN)
 * @return OK with NULL on success, or propagates error
 */
res_t ik_repl_flush_pending_scroll_arrow(ik_repl_ctx_t *repl, const ik_input_action_t *action)
{
    assert(repl != NULL);  /* LCOV_EXCL_BR_LINE */
    assert(action != NULL);  /* LCOV_EXCL_BR_LINE */
    assert(action->type != IK_INPUT_ARROW_UP);  /* LCOV_EXCL_BR_LINE */
    assert(action->type != IK_INPUT_ARROW_DOWN);  /* LCOV_EXCL_BR_LINE */
    assert(action->type != IK_INPUT_UNKNOWN);  /* LCOV_EXCL_BR_LINE */
    (void)action;  // Used only in asserts

    if (repl->scroll_det == NULL) {
        return OK(NULL);
    }

    ik_scroll_result_t flush_result = ik_scroll_detector_flush(repl->scroll_det);

    // LCOV_EXCL_START - Flush behavior requires precise timing control
    // If we flushed an arrow, handle it directly
    if (flush_result == IK_SCROLL_RESULT_ARROW_UP) {
        res_t r = ik_repl_handle_arrow_up_action(repl);
        if (is_err(&r)) return r;
    } else if (flush_result == IK_SCROLL_RESULT_ARROW_DOWN) {
        res_t r = ik_repl_handle_arrow_down_action(repl);
        if (is_err(&r)) return r;
    }
    // LCOV_EXCL_STOP

    return OK(NULL);
}

/**
 * @brief Handle ESC key press
 *
 * If agent is busy (WAITING_FOR_LLM or EXECUTING_TOOL), interrupts the operation.
 * Otherwise, dismisses completion if active (reverting to original input first).
 *
 * @param repl REPL context
 * @return OK with NULL on success, or propagates error
 */
res_t ik_repl_handle_escape_action(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);  /* LCOV_EXCL_BR_LINE */

    // Check if there's an in-flight operation to interrupt
    ik_agent_state_t state = atomic_load(&repl->current->state);

    // If agent is waiting or executing, interrupt it
    if (state == IK_AGENT_STATE_WAITING_FOR_LLM || state == IK_AGENT_STATE_EXECUTING_TOOL) {
        ik_repl_handle_interrupt_request(repl);
        return OK(NULL);
    }

    // Otherwise, handle ESC in IDLE mode (dismiss completion)
    // If completion is active, revert to original input before ESC dismisses
    if (repl->current->completion != NULL && repl->current->completion->original_input != NULL) {
        // Revert to original input
        const char *original = repl->current->completion->original_input;
        res_t res = ik_input_buffer_set_text(repl->current->input_buffer,
                                             original, strlen(original));
        if (is_err(&res)) {  // LCOV_EXCL_BR_LINE
            return res;  // LCOV_EXCL_LINE
        }
    }
    ik_repl_dismiss_completion(repl);
    return OK(NULL);
}

/**
 * @brief Handle user-initiated interrupt (ESC key)
 *
 * Sets interrupt flag and cancels in-flight operations:
 * - WAITING_FOR_LLM: Cancel HTTP stream via provider->cancel()
 * - EXECUTING_TOOL: Terminate child process group
 * - IDLE: No-op (nothing to interrupt)
 *
 * @param repl REPL context
 */
void ik_repl_handle_interrupt_request(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);  /* LCOV_EXCL_BR_LINE */
    assert(repl->current != NULL);  /* LCOV_EXCL_BR_LINE */

    ik_agent_ctx_t *agent = repl->current;

    // Check current state (thread-safe read)
    ik_agent_state_t state = atomic_load(&agent->state);

    // IDLE state: nothing to interrupt
    if (state == IK_AGENT_STATE_IDLE) {
        return;
    }

    // Set interrupt flag
    agent->interrupt_requested = true;

    // Cancel based on state
    if (state == IK_AGENT_STATE_WAITING_FOR_LLM) {
        // Cancel HTTP stream
        if (agent->provider_instance != NULL && agent->provider_instance->vt->cancel != NULL) {
            agent->provider_instance->vt->cancel(agent->provider_instance->ctx);
        }
    } else if (state == IK_AGENT_STATE_EXECUTING_TOOL) {
        // Terminate child process group
        if (agent->tool_child_pid > 0) {
            pid_t child_pid = agent->tool_child_pid;

            // Send SIGTERM to process group (negative PID)
            kill_(-child_pid, SIGTERM);

            // Wait up to 2 seconds for process to terminate
            const int32_t timeout_ms = 2000;
            const int32_t check_interval_ms = 100;
            int32_t elapsed_ms = 0;
            bool terminated = false;

            while (elapsed_ms < timeout_ms) {
                // Check if process has terminated (WNOHANG = non-blocking)
                int32_t status;
                pid_t result = waitpid_(child_pid, &status, WNOHANG);
                if (result == child_pid || result == -1) {
                    // Process terminated or doesn't exist
                    terminated = true;
                    break;
                }

                // Sleep for check interval
                usleep_((useconds_t)(check_interval_ms * 1000));
                elapsed_ms += check_interval_ms;
            }

            // Escalate to SIGKILL if process didn't terminate
            if (!terminated) {
                kill_(-child_pid, SIGKILL);
            }
        }
    }
}

res_t ik_repl_process_action(ik_repl_ctx_t *repl, const ik_input_action_t *action)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(action != NULL);   /* LCOV_EXCL_BR_LINE */

    // Intercept arrow up/down events for scroll detection (rel-05)
    if (action->type == IK_INPUT_ARROW_UP || action->type == IK_INPUT_ARROW_DOWN) {
        res_t r = ik_repl_process_scroll_detection(repl, action);
        if (is_err(&r)) return r;  // LCOV_EXCL_BR_LINE
        if (r.ok != NULL) return OK(NULL);  // Handled or buffered by scroll detector
    }

    // For non-arrow events, flush any pending arrow
    if (action->type != IK_INPUT_ARROW_UP &&
        action->type != IK_INPUT_ARROW_DOWN &&
        action->type != IK_INPUT_UNKNOWN) {
        res_t r = ik_repl_flush_pending_scroll_arrow(repl, action);
        if (is_err(&r)) return r;  // LCOV_EXCL_BR_LINE
    }

    switch (action->type) { // LCOV_EXCL_BR_LINE
        case IK_INPUT_CHAR: {
            // Handle Space when completion is active - commit selection and dismiss
            if (action->codepoint == ' ' && repl->current->completion != NULL) {
                return ik_repl_handle_completion_space_commit(repl);
            }

            if (repl->shared->history != NULL && ik_history_is_browsing(repl->shared->history)) {  // LCOV_EXCL_BR_LINE
                ik_history_stop_browsing(repl->shared->history);  // LCOV_EXCL_LINE
            }
            repl->current->viewport_offset = 0;

            // Insert the character first
            res_t res = ik_input_buffer_insert_codepoint(repl->current->input_buffer, action->codepoint);
            if (is_err(&res)) {
                return res;
            }

            // Update completion if active
            ik_repl_update_completion_after_char(repl);

            return OK(NULL);
        }
        case IK_INPUT_INSERT_NEWLINE:
            repl->current->viewport_offset = 0;
            return ik_input_buffer_insert_newline(repl->current->input_buffer);
        case IK_INPUT_NEWLINE:
            return ik_repl_handle_newline_action(repl);
        case IK_INPUT_BACKSPACE: {
            repl->current->viewport_offset = 0;
            res_t res = ik_input_buffer_backspace(repl->current->input_buffer);
            if (is_err(&res)) {  // LCOV_EXCL_BR_LINE
                return res;  // LCOV_EXCL_LINE
            }

            // Update completion if active
            ik_repl_update_completion_after_char(repl);

            return OK(NULL);
        }
        case IK_INPUT_DELETE:
            repl->current->viewport_offset = 0;
            return ik_input_buffer_delete(repl->current->input_buffer);
        case IK_INPUT_ARROW_LEFT:
            ik_repl_dismiss_completion(repl);
            repl->current->viewport_offset = 0;
            return ik_input_buffer_cursor_left(repl->current->input_buffer);
        case IK_INPUT_ARROW_RIGHT:
            ik_repl_dismiss_completion(repl);
            repl->current->viewport_offset = 0;
            return ik_input_buffer_cursor_right(repl->current->input_buffer);
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
            repl->current->viewport_offset = 0;
            return ik_input_buffer_cursor_to_line_start(repl->current->input_buffer);
        case IK_INPUT_CTRL_E:
            repl->current->viewport_offset = 0;
            return ik_input_buffer_cursor_to_line_end(repl->current->input_buffer);
        case IK_INPUT_CTRL_K:
            repl->current->viewport_offset = 0;
            return ik_input_buffer_kill_to_line_end(repl->current->input_buffer);
        case IK_INPUT_CTRL_N:
            // Disabled - history navigation will use Ctrl+R reverse search
            return OK(NULL);
        case IK_INPUT_CTRL_P:
            // Disabled - history navigation will use Ctrl+R reverse search
            return OK(NULL);
        case IK_INPUT_CTRL_U:
            repl->current->viewport_offset = 0;
            return ik_input_buffer_kill_line(repl->current->input_buffer);
        case IK_INPUT_CTRL_W:
            repl->current->viewport_offset = 0;
            return ik_input_buffer_delete_word_backward(repl->current->input_buffer);
        case IK_INPUT_CTRL_C:
            // Interrupt any in-flight operations first
            ik_repl_handle_interrupt_request(repl);
            // Then set quit flag to exit after cleanup
            repl->quit = true;
            return OK(NULL);
        case IK_INPUT_TAB:
            return ik_repl_handle_tab_action(repl);
        case IK_INPUT_ESCAPE:
            return ik_repl_handle_escape_action(repl);
        case IK_INPUT_NAV_PREV_SIBLING:
            return ik_repl_nav_prev_sibling(repl);
        case IK_INPUT_NAV_NEXT_SIBLING:
            return ik_repl_nav_next_sibling(repl);
        case IK_INPUT_NAV_PARENT:
            return ik_repl_nav_parent(repl);
        case IK_INPUT_NAV_CHILD:
            return ik_repl_nav_child(repl);
        case IK_INPUT_UNKNOWN:
            return OK(NULL);
        default: // LCOV_EXCL_LINE
            PANIC("Invalid input action type"); // LCOV_EXCL_LINE
    }
}
