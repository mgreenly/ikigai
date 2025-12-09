#include "repl.h"

#include "event_render.h"
#include "input_buffer/core.h"
#include "logger.h"
#include "openai/client_multi.h"
#include "panic.h"
#include "render_cursor.h"
#include "repl_event_handlers.h"
#include "shared.h"
#include "signal_handler.h"
#include "wrapper.h"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <talloc.h>
#include <time.h>

res_t ik_repl_run(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */

    // Initial render
    res_t result = ik_repl_render_frame(repl);
    if (is_err(&result)) {
        return result;
    }

    // Main event loop
    bool should_exit = false;
    while (!repl->quit && !should_exit) {  // LCOV_EXCL_BR_LINE
        // Check for pending resize
        CHECK(ik_signal_check_resize(repl));  // LCOV_EXCL_BR_LINE

        // Set up fd_sets
        fd_set read_fds, write_fds, exc_fds;
        int max_fd;
        CHECK(setup_fd_sets(repl, &read_fds, &write_fds, &exc_fds, &max_fd));  // LCOV_EXCL_BR_LINE

        // Add debug pipes to fd_set
        if (repl->shared->debug_mgr != NULL) {  // LCOV_EXCL_BR_LINE
            ik_debug_mgr_add_to_fdset(repl->shared->debug_mgr, &read_fds, &max_fd);  // LCOV_EXCL_LINE
        }

        // Calculate timeout
        long curl_timeout_ms = -1;
        CHECK(ik_openai_multi_timeout(repl->multi, &curl_timeout_ms));
        long effective_timeout_ms = calculate_select_timeout_ms(repl, curl_timeout_ms);

        struct timeval timeout;
        timeout.tv_sec = effective_timeout_ms / 1000;
        timeout.tv_usec = (effective_timeout_ms % 1000) * 1000;

        // Call select()
        int ready = posix_select_(max_fd + 1, &read_fds, &write_fds, &exc_fds, &timeout);

        if (ready < 0) {
            if (errno == EINTR) {
                CHECK(ik_signal_check_resize(repl));  // LCOV_EXCL_BR_LINE
                continue;
            }
            break;
        }

        // Handle timeout (spinner animation and arrow burst)
        // Note: Don't continue here - curl events must still be processed
        if (ready == 0) {
            // Advance spinner if visible
            if (repl->spinner_state.visible) {
                ik_spinner_advance(&repl->spinner_state);
                CHECK(ik_repl_render_frame(repl));
            }

            // Scroll accumulator has no timeout to check (synchronous algorithm)
        }

        // Handle debug pipes
        if (ready > 0 && repl->shared->debug_mgr != NULL) {  // LCOV_EXCL_BR_LINE
            ik_debug_mgr_handle_ready(repl->shared->debug_mgr, &read_fds, repl->scrollback, repl->shared->debug_enabled);  // LCOV_EXCL_LINE
        }

        if (FD_ISSET(repl->shared->term->tty_fd, &read_fds)) {  // LCOV_EXCL_BR_LINE
            CHECK(handle_terminal_input(repl, repl->shared->term->tty_fd, &should_exit));
            if (should_exit) break;
        }

        // Handle curl_multi events
        CHECK(handle_curl_events(repl, ready));  // LCOV_EXCL_BR_LINE

        // Poll for tool thread completion
        pthread_mutex_lock_(&repl->tool_thread_mutex);
        ik_repl_state_t current_state = repl->state;
        bool complete = repl->tool_thread_complete;
        pthread_mutex_unlock_(&repl->tool_thread_mutex);

        if (current_state == IK_REPL_STATE_EXECUTING_TOOL && complete) {
            handle_tool_completion(repl);
        }
    }

    return OK(NULL);
}

res_t ik_repl_submit_line(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */

    // Get current input buffer text
    const uint8_t *text_data = repl->input_buffer->text->data;
    size_t text_len = ik_byte_array_size(repl->input_buffer->text);

    // Add to history (skip empty input)
    if (text_len > 0 && repl->shared->history != NULL) {  // LCOV_EXCL_BR_LINE
        char *text = talloc_size(NULL, text_len + 1);
        if (text == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        memcpy(text, text_data, text_len);
        text[text_len] = '\0';

        // Add to history structure (with deduplication)
        res_t result = ik_history_add(repl->shared->history, text);
        if (is_err(&result)) {  // LCOV_EXCL_BR_LINE
            talloc_free(text);  // LCOV_EXCL_LINE
            return result;  // LCOV_EXCL_LINE
        }

        // Append to history file
        result = ik_history_append_entry(text);
        if (is_err(&result)) {  // LCOV_EXCL_BR_LINE - File IO errors tested in history file_io_errors_test.c
            // Log warning but continue (file write failure shouldn't block REPL)
            ik_log_warn("Failed to append to history file: %s", result.err->msg);  // LCOV_EXCL_LINE
            talloc_free(result.err);  // LCOV_EXCL_LINE
        }

        talloc_free(text);

        // Exit browsing mode if active
        if (ik_history_is_browsing(repl->shared->history)) {  // LCOV_EXCL_BR_LINE
            ik_history_stop_browsing(repl->shared->history);  // LCOV_EXCL_LINE
        }
    }

    // Render user message via event renderer
    if (text_len > 0 && repl->scrollback != NULL) {  // LCOV_EXCL_BR_LINE
        char *text = talloc_size(NULL, text_len + 1);
        if (text == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        memcpy(text, text_data, text_len);
        text[text_len] = '\0';
        res_t result = ik_event_render(repl->scrollback, "user", text, "{}");
        talloc_free(text);
        if (is_err(&result)) return result;
    }

    ik_input_buffer_clear(repl->input_buffer);
    repl->viewport_offset = 0; // Auto-scroll to bottom

    return OK(NULL);
}

res_t ik_repl_handle_resize(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->shared->term != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->scrollback != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->input_buffer != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->shared->render != NULL);   /* LCOV_EXCL_BR_LINE */

    int rows, cols;
    res_t result = ik_term_get_size(repl->shared->term, &rows, &cols);
    if (is_err(&result)) return result;

    repl->shared->render->rows = rows;
    repl->shared->render->cols = cols;

    ik_scrollback_ensure_layout(repl->scrollback, cols);
    ik_input_buffer_ensure_layout(repl->input_buffer, cols);

    // Trigger immediate redraw with new dimensions
    return ik_repl_render_frame(repl);
}

void ik_repl_transition_to_waiting_for_llm(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */

    // Update state with mutex protection for thread safety
    pthread_mutex_lock_(&repl->tool_thread_mutex);
    assert(repl->state == IK_REPL_STATE_IDLE);   /* LCOV_EXCL_BR_LINE */
    repl->state = IK_REPL_STATE_WAITING_FOR_LLM;
    pthread_mutex_unlock_(&repl->tool_thread_mutex);

    // Show spinner, hide input
    repl->spinner_state.visible = true;
    repl->input_buffer_visible = false;
}

void ik_repl_transition_to_idle(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */

    // Update state with mutex protection for thread safety
    pthread_mutex_lock_(&repl->tool_thread_mutex);
    assert(repl->state == IK_REPL_STATE_WAITING_FOR_LLM);   /* LCOV_EXCL_BR_LINE */
    repl->state = IK_REPL_STATE_IDLE;
    pthread_mutex_unlock_(&repl->tool_thread_mutex);

    // Hide spinner, show input
    repl->spinner_state.visible = false;
    repl->input_buffer_visible = true;
}

void ik_repl_transition_to_executing_tool(ik_repl_ctx_t *repl)
{
    assert(repl != NULL); /* LCOV_EXCL_BR_LINE */
    pthread_mutex_lock_(&repl->tool_thread_mutex);
    assert(repl->state == IK_REPL_STATE_WAITING_FOR_LLM); /* LCOV_EXCL_BR_LINE */
    repl->state = IK_REPL_STATE_EXECUTING_TOOL;
    pthread_mutex_unlock_(&repl->tool_thread_mutex);
}

void ik_repl_transition_from_executing_tool(ik_repl_ctx_t *repl)
{
    assert(repl != NULL); /* LCOV_EXCL_BR_LINE */
    pthread_mutex_lock_(&repl->tool_thread_mutex);
    assert(repl->state == IK_REPL_STATE_EXECUTING_TOOL); /* LCOV_EXCL_BR_LINE */
    repl->state = IK_REPL_STATE_WAITING_FOR_LLM;
    pthread_mutex_unlock_(&repl->tool_thread_mutex);
}

bool ik_repl_should_continue_tool_loop(const ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */

    /* Check if finish_reason is "tool_calls" */
    if (repl->response_finish_reason == NULL) {
        return false;
    }

    if (strcmp(repl->response_finish_reason, "tool_calls") != 0) {
        return false;
    }

    /* Check if we've reached the tool iteration limit (if config is available) */
    if (repl->shared->cfg != NULL && repl->tool_iteration_count >= repl->shared->cfg->max_tool_turns) {
        return false;
    }

    return true;
}
