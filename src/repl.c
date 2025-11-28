#include "repl.h"

#include "db/message.h"
#include "event_render.h"
#include "format.h"
#include "input_buffer/core.h"
#include "openai/client_multi.h"
#include "panic.h"
#include "render_cursor.h"
#include "repl_actions.h"
#include "signal_handler.h"
#include "wrapper.h"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <talloc.h>
#include <time.h>

// Helper: Calculate effective select() timeout
static inline long calculate_select_timeout_ms(const ik_repl_ctx_t *repl, long curl_timeout_ms)
{
    // Spinner timer: 80ms when visible, no timeout when hidden
    long spinner_timeout_ms = repl->spinner_state.visible ? 80 : -1;  // LCOV_EXCL_BR_LINE

    // Use minimum of both timeouts (if both are set)
    if (spinner_timeout_ms >= 0 && curl_timeout_ms >= 0) {  // LCOV_EXCL_BR_LINE
        return (spinner_timeout_ms < curl_timeout_ms) ? spinner_timeout_ms : curl_timeout_ms;  // LCOV_EXCL_LINE
    }
    if (spinner_timeout_ms >= 0) {  // LCOV_EXCL_BR_LINE
        return spinner_timeout_ms;  // LCOV_EXCL_LINE
    }
    if (curl_timeout_ms >= 0) {  // LCOV_EXCL_BR_LINE
        return curl_timeout_ms;  // LCOV_EXCL_LINE
    }
    // Use a 1-second timeout to prevent blocking forever
    return 1000;
}

// Helper: Set up file descriptor sets for select()
static inline res_t setup_fd_sets(ik_repl_ctx_t *repl,
                                  fd_set *read_fds,
                                  fd_set *write_fds,
                                  fd_set *exc_fds,
                                  int *max_fd_out)
{
    FD_ZERO(read_fds);
    FD_ZERO(write_fds);
    FD_ZERO(exc_fds);

    // Add terminal fd
    int32_t terminal_fd = repl->term->tty_fd;
    FD_SET(terminal_fd, read_fds);
    int max_fd = terminal_fd;

    // Add curl_multi fds
    int curl_max_fd = -1;
    res_t result = ik_openai_multi_fdset(repl->multi, read_fds, write_fds, exc_fds, &curl_max_fd);
    if (is_err(&result)) {
        return result;
    }
    if (curl_max_fd > max_fd) {  // LCOV_EXCL_BR_LINE
        max_fd = curl_max_fd;  // LCOV_EXCL_LINE
    }

    *max_fd_out = max_fd;
    return OK(NULL);
}

// Helper: Handle terminal input
// Exposed for testing
res_t handle_terminal_input(ik_repl_ctx_t *repl, int terminal_fd, bool *should_exit)
{
    char byte;
    ssize_t n = posix_read_(terminal_fd, &byte, 1);
    if (n < 0) {
        if (errno == EINTR) {
            return OK(NULL);
        }
        *should_exit = true;
        return OK(NULL);
    }
    if (n == 0) {
        *should_exit = true;
        return OK(NULL);
    }

    // Parse and process action
    ik_input_action_t action;
    ik_input_parse_byte(repl->input_parser, byte, &action);

    res_t result = ik_repl_process_action(repl, &action);
    // Error propagation: Currently unreachable through normal terminal input because
    // the input parser (input.c:decode_utf8_sequence) sanitizes invalid codepoints
    // to U+FFFD before they reach ik_repl_process_action(). The error handling in
    // ik_repl_process_action itself IS tested (see test_repl_process_action_invalid_codepoint).
    // This defensive check remains for future robustness if new failable actions are added.
    if (is_err(&result)) return result; // LCOV_EXCL_BR_LINE

    // Render if needed
    if (action.type != IK_INPUT_UNKNOWN) {
        return ik_repl_render_frame(repl);
    }

    return OK(NULL);
}

// Helper: Handle HTTP request error
static void handle_request_error(ik_repl_ctx_t *repl)
{
    // Display error in scrollback
    const char *error_prefix = "Error: ";
    size_t prefix_len = strlen(error_prefix);
    size_t error_len = strlen(repl->http_error_message);
    char *full_error = talloc_zero_(repl, prefix_len + error_len + 1);
    if (full_error == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    memcpy(full_error, error_prefix, prefix_len);
    memcpy(full_error + prefix_len, repl->http_error_message, error_len);
    full_error[prefix_len + error_len] = '\0';

    ik_scrollback_append_line(repl->scrollback, full_error, prefix_len + error_len);
    talloc_free(full_error);

    // Clear error message
    talloc_free(repl->http_error_message);
    repl->http_error_message = NULL;

    // Clear accumulated assistant response (partial response on error)
    if (repl->assistant_response != NULL) {
        talloc_free(repl->assistant_response);
        repl->assistant_response = NULL;
    }
}

// Helper: Handle HTTP request success
// Exposed for testing
void handle_request_success(ik_repl_ctx_t *repl)
{
    // Add assistant response to conversation
    if (repl->assistant_response != NULL && strlen(repl->assistant_response) > 0) {
        ik_openai_msg_t *assistant_msg = ik_openai_msg_create(repl->conversation,
                                                              "assistant",
                                                              repl->assistant_response).ok;
        res_t result = ik_openai_conversation_add_msg(repl->conversation, assistant_msg);
        if (is_err(&result)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE

        // Persist assistant message to database (Integration Point 2)
        if (repl->db_ctx != NULL && repl->current_session_id > 0) {
            // Build data JSON with response metadata
            char *data_json = talloc_strdup(repl, "{");
            bool first = true;

            if (repl->response_model != NULL) {
                data_json = talloc_asprintf_append(data_json, "\"model\":\"%s\"", repl->response_model);
                first = false;
            }
            if (repl->response_completion_tokens > 0) {
                data_json = talloc_asprintf_append(data_json,
                                                   "%s\"tokens\":%d",
                                                   first ? "" : ",",
                                                   repl->response_completion_tokens);
                first = false;
            }
            if (repl->response_finish_reason != NULL) {
                data_json = talloc_asprintf_append(data_json,
                                                   "%s\"finish_reason\":\"%s\"",
                                                   first ? "" : ",",
                                                   repl->response_finish_reason);
            }
            data_json = talloc_strdup_append(data_json, "}");

            res_t db_res = ik_db_message_insert_(repl->db_ctx, repl->current_session_id,
                                                 "assistant", repl->assistant_response, data_json);
            if (is_err(&db_res)) {
                // Log error but don't crash - memory state is authoritative
                if (repl->db_debug_pipe != NULL && repl->db_debug_pipe->write_end != NULL) {
                    fprintf(repl->db_debug_pipe->write_end,
                            "Warning: Failed to persist assistant message to database: %s\n",
                            error_message(db_res.err));
                }
                talloc_free(db_res.err);
            }
            talloc_free(data_json);
        }

        // Clear the assistant response
        talloc_free(repl->assistant_response);
        repl->assistant_response = NULL;
    }
}

// Helper: Handle curl_multi events and detect request completion
// Exposed for testing
res_t handle_curl_events(ik_repl_ctx_t *repl, int ready)
{
    // Only call curl_multi_perform when there's work to do:
    // - ready == 0 means select() timed out (curl may need to handle its timeouts)
    // - curl_still_running > 0 means there are active transfers that need processing
    if (ready == 0 || repl->curl_still_running > 0) {
        int prev_running = repl->curl_still_running;
        CHECK(ik_openai_multi_perform(repl->multi, &repl->curl_still_running));
        ik_openai_multi_info_read(repl->multi);

        // Detect request completion (was running, now not running)
        if (prev_running > 0 && repl->curl_still_running == 0 && repl->state == IK_REPL_STATE_WAITING_FOR_LLM) {
            // Check if request failed (error message set by completion callback)
            if (repl->http_error_message != NULL) {
                handle_request_error(repl);
            } else {
                handle_request_success(repl);
            }

            // Transition back to IDLE state
            ik_repl_transition_to_idle(repl);

            // Trigger re-render to update UI
            CHECK(ik_repl_render_frame(repl));
        }
    }
    return OK(NULL);
}

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
        if (repl->debug_mgr != NULL) {  // LCOV_EXCL_BR_LINE
            ik_debug_mgr_add_to_fdset(repl->debug_mgr, &read_fds, &max_fd);  // LCOV_EXCL_LINE
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

        // Handle timeout (spinner animation)
        if (ready == 0 && repl->spinner_state.visible) {
            ik_spinner_advance(&repl->spinner_state);
            CHECK(ik_repl_render_frame(repl));
            continue;
        }

        // Handle debug pipes
        if (ready > 0 && repl->debug_mgr != NULL) {  // LCOV_EXCL_BR_LINE
            ik_debug_mgr_handle_ready(repl->debug_mgr, &read_fds, repl->scrollback, repl->debug_enabled);  // LCOV_EXCL_LINE
        }

        // Handle terminal input
        if (FD_ISSET(repl->term->tty_fd, &read_fds)) {  // LCOV_EXCL_BR_LINE
            CHECK(handle_terminal_input(repl, repl->term->tty_fd, &should_exit));
            if (should_exit) break;
        }

        // Handle curl_multi events
        CHECK(handle_curl_events(repl, ready));  // LCOV_EXCL_BR_LINE
    }

    return OK(NULL);
}

res_t ik_repl_submit_line(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */

    // Get current input buffer text
    const uint8_t *text_data = repl->input_buffer->text->data;
    size_t text_len = ik_byte_array_size(repl->input_buffer->text);

    // Render user message via event renderer (only if there's content and scrollback exists)
    // Using event_render ensures consistency between live entry and replay
    if (text_len > 0 && repl->scrollback != NULL) {  // LCOV_EXCL_BR_LINE
        // Create null-terminated copy - input buffer data is NOT null-terminated
        // ik_event_render() expects null-terminated strings (uses strlen internally)
        char *text = talloc_size(NULL, text_len + 1);
        if (text == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        memcpy(text, text_data, text_len);
        text[text_len] = '\0';

        res_t result = ik_event_render(repl->scrollback, "user", text, "{}");
        talloc_free(text);
        if (is_err(&result)) {
            return result;
        }
    }

    // Clear input buffer
    ik_input_buffer_clear(repl->input_buffer);

    // Auto-scroll to bottom (reset viewport offset)
    repl->viewport_offset = 0;

    return OK(NULL);
}

res_t ik_repl_handle_resize(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->term != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->scrollback != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->input_buffer != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->render != NULL);   /* LCOV_EXCL_BR_LINE */

    // Update terminal dimensions
    int rows, cols;
    res_t result = ik_term_get_size(repl->term, &rows, &cols);
    if (is_err(&result)) {
        return result;
    }

    // Update render context dimensions
    repl->render->rows = rows;
    repl->render->cols = cols;

    // Invalidate layout caches (will recalculate on next ensure_layout call)
    ik_scrollback_ensure_layout(repl->scrollback, cols);
    ik_input_buffer_ensure_layout(repl->input_buffer, cols);

    // Trigger immediate redraw with new dimensions
    return ik_repl_render_frame(repl);
}

void ik_repl_transition_to_waiting_for_llm(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->state == IK_REPL_STATE_IDLE);   /* LCOV_EXCL_BR_LINE */

    // Update state
    repl->state = IK_REPL_STATE_WAITING_FOR_LLM;

    // Show spinner, hide input
    repl->spinner_state.visible = true;
    repl->input_buffer_visible = false;
}

void ik_repl_transition_to_idle(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->state == IK_REPL_STATE_WAITING_FOR_LLM);   /* LCOV_EXCL_BR_LINE */

    // Update state
    repl->state = IK_REPL_STATE_IDLE;

    // Hide spinner, show input
    repl->spinner_state.visible = false;
    repl->input_buffer_visible = true;
}
