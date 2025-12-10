#include "repl_event_handlers.h"

#include "db/message.h"
#include "event_render.h"
#include "input.h"
#include "logger.h"
#include "openai/client_multi.h"
#include "panic.h"
#include "repl.h"
#include "repl_actions.h"
#include "repl_callbacks.h"
#include "shared.h"
#include "wrapper.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <talloc.h>
#include <time.h>

// Forward declarations
static void submit_tool_loop_continuation(ik_repl_ctx_t *repl);
static void persist_assistant_msg(ik_repl_ctx_t *repl);

long calculate_select_timeout_ms(ik_repl_ctx_t *repl, long curl_timeout_ms)
{
    // Spinner timer: 80ms when visible, no timeout when hidden
    long spinner_timeout_ms = repl->spinner_state.visible ? 80 : -1;  // LCOV_EXCL_BR_LINE

    // Tool polling: 50ms when executing tool to detect completion quickly
    pthread_mutex_lock_(&repl->tool_thread_mutex);
    ik_repl_state_t current_state = repl->state;
    pthread_mutex_unlock_(&repl->tool_thread_mutex);
    long tool_poll_timeout_ms = (current_state == IK_REPL_STATE_EXECUTING_TOOL) ? 50 : -1;

    // Scroll detector timeout: get time until pending arrow must flush
    long scroll_timeout_ms = -1;
    if (repl->scroll_det != NULL) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        int64_t now_ms = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
        scroll_timeout_ms = ik_scroll_detector_get_timeout_ms(repl->scroll_det, now_ms);
    }

    // Collect all timeouts
    long timeouts[] = {spinner_timeout_ms, curl_timeout_ms, tool_poll_timeout_ms, scroll_timeout_ms};
    long min_timeout = -1;

    for (size_t i = 0; i < sizeof(timeouts) / sizeof(timeouts[0]); i++) {
        if (timeouts[i] >= 0) {
            if (min_timeout < 0 || timeouts[i] < min_timeout) {
                min_timeout = timeouts[i];
            }
        }
    }

    if (min_timeout >= 0) {
        return min_timeout;
    }
    // Use a 1-second timeout to prevent blocking forever
    return 1000;
}

res_t setup_fd_sets(ik_repl_ctx_t *repl,
                    fd_set *read_fds,
                    fd_set *write_fds,
                    fd_set *exc_fds,
                    int *max_fd_out)
{
    FD_ZERO(read_fds);
    FD_ZERO(write_fds);
    FD_ZERO(exc_fds);

    // Add terminal fd
    int32_t terminal_fd = repl->shared->term->tty_fd;
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
    // Unreachable: input parser sanitizes codepoints. (See test_repl_process_action_invalid_codepoint)
    if (is_err(&result)) return result; // LCOV_EXCL_BR_LINE

    // Render if needed
    if (action.type != IK_INPUT_UNKNOWN) {
        return ik_repl_render_frame(repl);
    }

    return OK(NULL);
}

// Persist assistant message to database
static void persist_assistant_msg(ik_repl_ctx_t *repl)
{
    if (repl->shared->db_ctx == NULL || repl->shared->session_id <= 0) return;

    char *data_json = talloc_strdup(repl, "{");
    bool first = true;

    if (repl->response_model != NULL) {
        data_json = talloc_asprintf_append(data_json, "\"model\":\"%s\"", repl->response_model);
        first = false;
    }
    if (repl->response_completion_tokens > 0) {
        data_json = talloc_asprintf_append(data_json, "%s\"tokens\":%d",
                                           first ? "" : ",", repl->response_completion_tokens);
        first = false;
    }
    if (repl->response_finish_reason != NULL) {
        data_json = talloc_asprintf_append(data_json, "%s\"finish_reason\":\"%s\"",
                                           first ? "" : ",", repl->response_finish_reason);
    }
    data_json = talloc_strdup_append(data_json, "}");

    res_t db_res = ik_db_message_insert_(repl->shared->db_ctx, repl->shared->session_id,
                                         "assistant", repl->assistant_response, data_json);
    if (is_err(&db_res)) {
        if (repl->shared->db_debug_pipe != NULL && repl->shared->db_debug_pipe->write_end != NULL) {
            fprintf(repl->shared->db_debug_pipe->write_end,
                    "Warning: Failed to persist assistant message to database: %s\n",
                    error_message(db_res.err));
        }
        talloc_free(db_res.err);
    }
    talloc_free(data_json);
}

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

void handle_request_success(ik_repl_ctx_t *repl)
{
    // Add assistant response to conversation (only if non-empty)
    if (repl->assistant_response != NULL && strlen(repl->assistant_response) > 0) {
        ik_msg_t *assistant_msg = ik_openai_msg_create(repl->conversation,
                                                "assistant",
                                                repl->assistant_response).ok;
        res_t result = ik_openai_conversation_add_msg(repl->conversation, assistant_msg);
        if (is_err(&result)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE

        if (repl->shared->openai_debug_pipe && repl->shared->openai_debug_pipe->write_end) {
            size_t len = strlen(repl->assistant_response);
            fprintf(repl->shared->openai_debug_pipe->write_end,
                    len > 80 ? "<< ASSISTANT: %.77s...\n" : "<< ASSISTANT: %s\n",
                    repl->assistant_response);
            fflush(repl->shared->openai_debug_pipe->write_end);
        }
        persist_assistant_msg(repl);
    }

    // Clear the assistant response (whether empty or not)
    if (repl->assistant_response != NULL) {
        talloc_free(repl->assistant_response);
        repl->assistant_response = NULL;
    }

    // Execute pending tool call (async)
    if (repl->pending_tool_call != NULL) {
        ik_repl_start_tool_execution(repl);
        return; // Exit early - completion handled in event loop
    }

    // No tool call - check if tool loop should continue
    if (ik_repl_should_continue_tool_loop(repl)) {
        repl->tool_iteration_count++;
        submit_tool_loop_continuation(repl);
    }
}

static void submit_tool_loop_continuation(ik_repl_ctx_t *repl)
{
    bool limit_reached = (repl->shared->cfg != NULL && repl->tool_iteration_count >= repl->shared->cfg->max_tool_turns);  // LCOV_EXCL_BR_LINE
    res_t result = ik_openai_multi_add_request(repl->multi, repl->shared->cfg, repl->conversation,
                                               ik_repl_streaming_callback, repl,
                                               ik_repl_http_completion_callback, repl,
                                               limit_reached);
    if (is_err(&result)) {  // LCOV_EXCL_BR_LINE
        const char *err_msg = error_message(result.err);  // LCOV_EXCL_LINE
        ik_scrollback_append_line(repl->scrollback, err_msg, strlen(err_msg));  // LCOV_EXCL_LINE
        ik_repl_transition_to_idle(repl);  // LCOV_EXCL_LINE
        talloc_free(result.err);  // LCOV_EXCL_LINE
    } else {
        repl->curl_still_running = 1;
    }
}

// Handle curl events

res_t handle_curl_events(ik_repl_ctx_t *repl, int ready)
{
    (void)ready;  // Used for select() coordination, not needed here

    // Only process when there are active transfers
    if (repl->curl_still_running > 0) {
        int prev_running = repl->curl_still_running;
        CHECK(ik_openai_multi_perform(repl->multi, &repl->curl_still_running));
        ik_openai_multi_info_read(repl->multi);

        // Detect request completion (was running, now not running)
        pthread_mutex_lock_(&repl->tool_thread_mutex);
        ik_repl_state_t current_state = repl->state;
        pthread_mutex_unlock_(&repl->tool_thread_mutex);

        if (prev_running > 0 && repl->curl_still_running == 0 && current_state == IK_REPL_STATE_WAITING_FOR_LLM) {  // LCOV_EXCL_BR_LINE
            // Check if request failed (error message set by completion callback)
            if (repl->http_error_message != NULL) {
                handle_request_error(repl);
            } else {
                handle_request_success(repl);
            }

            // Transition back to IDLE state only if we're still WAITING_FOR_LLM.
            // If handle_request_success started a tool execution, state is now EXECUTING_TOOL
            // and we should NOT transition to IDLE.
            pthread_mutex_lock_(&repl->tool_thread_mutex);
            current_state = repl->state;
            pthread_mutex_unlock_(&repl->tool_thread_mutex);

            if (current_state == IK_REPL_STATE_WAITING_FOR_LLM) {
                ik_repl_transition_to_idle(repl);
            }

            // Trigger re-render to update UI
            CHECK(ik_repl_render_frame(repl));
        }
    }
    return OK(NULL);
}

// Handle tool thread completion - extracted from event loop
// Non-static for testing
void handle_tool_completion(ik_repl_ctx_t *repl)
{
    // Thread finished - harvest result and continue
    ik_repl_complete_tool_execution(repl);

    // Check if tool loop should continue
    if (ik_repl_should_continue_tool_loop(repl)) {
        repl->tool_iteration_count++;
        submit_tool_loop_continuation(repl);
    } else {
        // Tool loop done - transition to IDLE, show input prompt
        ik_repl_transition_to_idle(repl);
    }

    // Re-render to show tool result in scrollback
    res_t result = ik_repl_render_frame(repl);
    if (is_err(&result)) PANIC("render failed"); // LCOV_EXCL_BR_LINE
}
