#include "repl_event_handlers.h"

#include "agent.h"
#include "db/message.h"
#include "event_render.h"
#include "input.h"
#include "layer_wrappers.h"
#include "logger.h"
#include "openai/client_multi.h"
#include "panic.h"
#include "repl.h"
#include "repl_actions.h"
#include "repl_actions_internal.h"
#include "repl_callbacks.h"
#include "scroll_detector.h"
#include "shared.h"
#include "wrapper.h"

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <talloc.h>
#include <time.h>

// Forward declarations
static void submit_tool_loop_continuation(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent);
static void persist_assistant_msg(ik_repl_ctx_t *repl);

long ik_repl_calculate_select_timeout_ms(ik_repl_ctx_t *repl, long curl_timeout_ms)
{
    // Spinner timer: 80ms when visible, no timeout when hidden
    long spinner_timeout_ms = repl->current->spinner_state.visible ? 80 : -1;  // LCOV_EXCL_BR_LINE

    // Tool polling: check ALL agents for executing tools
    long tool_poll_timeout_ms = -1;
    for (size_t i = 0; i < repl->agent_count; i++) {
        ik_agent_ctx_t *agent = repl->agents[i];
        pthread_mutex_lock_(&agent->tool_thread_mutex);
        bool executing = (agent->state == IK_AGENT_STATE_EXECUTING_TOOL);
        pthread_mutex_unlock_(&agent->tool_thread_mutex);
        if (executing) {
            tool_poll_timeout_ms = 50;
            break;  // Found one, no need to check more
        }
    }

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

res_t ik_repl_setup_fd_sets(ik_repl_ctx_t *repl,
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

    // Add curl_multi fds for ALL agents
    for (size_t i = 0; i < repl->agent_count; i++) {
        ik_agent_ctx_t *agent = repl->agents[i];
        int agent_max_fd = -1;
        res_t result = ik_openai_multi_fdset(agent->multi, read_fds, write_fds, exc_fds, &agent_max_fd);
        if (is_err(&result)) return result;
        if (agent_max_fd > max_fd) {
            max_fd = agent_max_fd;
        }
    }

    *max_fd_out = max_fd;
    return OK(NULL);
}

res_t ik_repl_handle_terminal_input(ik_repl_ctx_t *repl, int terminal_fd, bool *should_exit)
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

    // Capture render start time (input received)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    repl->render_start_us = (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;

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

    if (repl->current->response_model != NULL) {
        data_json = talloc_asprintf_append(data_json, "\"model\":\"%s\"", repl->current->response_model);
        first = false;
    }
    if (repl->current->response_completion_tokens > 0) {
        data_json = talloc_asprintf_append(data_json, "%s\"tokens\":%d",
                                           first ? "" : ",", repl->current->response_completion_tokens);
        first = false;
    }
    if (repl->current->response_finish_reason != NULL) {
        data_json = talloc_asprintf_append(data_json, "%s\"finish_reason\":\"%s\"",
                                           first ? "" : ",", repl->current->response_finish_reason);
    }
    data_json = talloc_strdup_append(data_json, "}");

    res_t db_res = ik_db_message_insert_(repl->shared->db_ctx, repl->shared->session_id,
                                         repl->current->uuid, "assistant", repl->current->assistant_response, data_json);
    if (is_err(&db_res)) {
        yyjson_mut_doc *log_doc = ik_log_create();  // LCOV_EXCL_LINE
        yyjson_mut_val *root = yyjson_mut_doc_get_root(log_doc);  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, root, "event", "db_persist_failed");  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, root, "operation", "persist_assistant_msg");  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, root, "error", error_message(db_res.err));  // LCOV_EXCL_LINE
        ik_logger_warn_json(repl->shared->logger, log_doc);  // LCOV_EXCL_LINE
        talloc_free(db_res.err);  // LCOV_EXCL_LINE
    }
    talloc_free(data_json);
}

static void handle_agent_request_error(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent)
{
    // Display error in scrollback
    const char *error_prefix = "Error: ";
    size_t prefix_len = strlen(error_prefix);
    size_t error_len = strlen(agent->http_error_message);
    char *full_error = talloc_zero_(repl, prefix_len + error_len + 1);
    if (full_error == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    memcpy(full_error, error_prefix, prefix_len);
    memcpy(full_error + prefix_len, agent->http_error_message, error_len);
    full_error[prefix_len + error_len] = '\0';

    ik_scrollback_append_line(agent->scrollback, full_error, prefix_len + error_len);
    talloc_free(full_error);

    // Clear error message
    talloc_free(agent->http_error_message);
    agent->http_error_message = NULL;

    // Clear accumulated assistant response (partial response on error)
    if (agent->assistant_response != NULL) {
        talloc_free(agent->assistant_response);
        agent->assistant_response = NULL;
    }
}

void ik_repl_handle_agent_request_success(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent)
{
    // Add assistant response to conversation (only if non-empty)
    if (agent->assistant_response != NULL && strlen(agent->assistant_response) > 0) {
        ik_msg_t *assistant_msg = ik_openai_msg_create(agent->conversation,
                                                "assistant",
                                                agent->assistant_response).ok;
        res_t result = ik_openai_conversation_add_msg(agent->conversation, assistant_msg);
        if (is_err(&result)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE

        yyjson_mut_doc *log_doc = ik_log_create();  // LCOV_EXCL_LINE
        yyjson_mut_val *root = yyjson_mut_doc_get_root(log_doc);  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, root, "event", "assistant_response");  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_int(log_doc, root, "length", (int64_t)strlen(agent->assistant_response));  // LCOV_EXCL_LINE
        // Truncate long responses for log readability (same as original behavior)
        if (strlen(agent->assistant_response) > 80) {  // LCOV_EXCL_BR_LINE
            char truncated[81];  // LCOV_EXCL_LINE
            snprintf(truncated, sizeof(truncated), "%.77s...", agent->assistant_response);  // LCOV_EXCL_LINE
            yyjson_mut_obj_add_str(log_doc, root, "preview", truncated);  // LCOV_EXCL_LINE
        } else {  // LCOV_EXCL_LINE
            yyjson_mut_obj_add_str(log_doc, root, "content", agent->assistant_response);  // LCOV_EXCL_LINE
        }  // LCOV_EXCL_LINE
        ik_logger_debug_json(repl->shared->logger, log_doc);  // LCOV_EXCL_LINE
        persist_assistant_msg(repl);
    }

    // Clear the assistant response (whether empty or not)
    if (agent->assistant_response != NULL) {
        talloc_free(agent->assistant_response);
        agent->assistant_response = NULL;
    }

    // Execute pending tool call (async)
    if (agent->pending_tool_call != NULL) {
        ik_agent_start_tool_execution(agent);
        return; // Exit early - completion handled in event loop
    }

    // No tool call - check if tool loop should continue
    if (ik_agent_should_continue_tool_loop(agent)) {
        agent->tool_iteration_count++;
        submit_tool_loop_continuation(repl, agent);
    }
}

static void submit_tool_loop_continuation(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent)
{
    bool limit_reached = (repl->shared->cfg != NULL && agent->tool_iteration_count >= repl->shared->cfg->max_tool_turns);  // LCOV_EXCL_BR_LINE
    res_t result = ik_openai_multi_add_request(agent->multi, repl->shared->cfg, agent->conversation,
                                               ik_repl_streaming_callback, agent,
                                               ik_repl_http_completion_callback, agent,
                                               limit_reached,
                                               repl->shared->logger);
    if (is_err(&result)) {  // LCOV_EXCL_BR_LINE
        const char *err_msg = error_message(result.err);  // LCOV_EXCL_LINE
        ik_scrollback_append_line(agent->scrollback, err_msg, strlen(err_msg));  // LCOV_EXCL_LINE
        ik_agent_transition_to_idle(agent);  // LCOV_EXCL_LINE
        talloc_free(result.err);  // LCOV_EXCL_LINE
    } else {
        agent->curl_still_running = 1;
    }
}

// Handle curl events

// Helper function to process a single agent's HTTP events
static res_t process_agent_curl_events(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent)
{
    if (agent->curl_still_running > 0) {
        int prev_running = agent->curl_still_running;
        CHECK(ik_openai_multi_perform(agent->multi, &agent->curl_still_running));
        ik_openai_multi_info_read(agent->multi, repl->shared->logger);

        // Detect request completion (was running, now not running)
        pthread_mutex_lock_(&agent->tool_thread_mutex);
        ik_agent_state_t current_state = agent->state;
        pthread_mutex_unlock_(&agent->tool_thread_mutex);

        if (prev_running > 0 && agent->curl_still_running == 0 && current_state == IK_AGENT_STATE_WAITING_FOR_LLM) {  // LCOV_EXCL_BR_LINE
            // Check if request failed (error message set by completion callback)
            if (agent->http_error_message != NULL) {
                handle_agent_request_error(repl, agent);
            } else {
                ik_repl_handle_agent_request_success(repl, agent);
            }

            // Transition back to IDLE state only if we're still WAITING_FOR_LLM.
            // If ik_repl_handle_agent_request_success started a tool execution, state is now EXECUTING_TOOL
            // and we should NOT transition to IDLE.
            pthread_mutex_lock_(&agent->tool_thread_mutex);
            current_state = agent->state;
            pthread_mutex_unlock_(&agent->tool_thread_mutex);

            if (current_state == IK_AGENT_STATE_WAITING_FOR_LLM) {
                ik_agent_transition_to_idle(agent);
            }

            // Only render if this is the current agent
            if (agent == repl->current) {
                CHECK(ik_repl_render_frame(repl));
            }
        }
    }
    return OK(NULL);
}

res_t ik_repl_handle_curl_events(ik_repl_ctx_t *repl, int ready)
{
    (void)ready;  // Used for select() coordination, not needed here

    // Process ALL agents with active transfers
    for (size_t i = 0; i < repl->agent_count; i++) {
        ik_agent_ctx_t *agent = repl->agents[i];
        CHECK(process_agent_curl_events(repl, agent));
    }

    // Also process current agent if it's not in the agents array (for tests/single-agent mode)
    if (repl->current != NULL) {
        bool current_in_array = false;
        for (size_t i = 0; i < repl->agent_count; i++) {
            if (repl->agents[i] == repl->current) {
                current_in_array = true;
                break;
            }
        }
        if (!current_in_array) {
            CHECK(process_agent_curl_events(repl, repl->current));
        }
    }

    return OK(NULL);
}

// Handle tool thread completion - extracted from event loop
// Non-static for testing
void ik_repl_handle_agent_tool_completion(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent)
{
    // Thread finished - harvest result and continue
    ik_agent_complete_tool_execution(agent);

    // Check if tool loop should continue
    if (ik_agent_should_continue_tool_loop(agent)) {
        agent->tool_iteration_count++;
        submit_tool_loop_continuation(repl, agent);
    } else {
        // Tool loop done - transition to IDLE, show input prompt
        ik_agent_transition_to_idle(agent);
    }

    // Only render if this is the current agent
    if (agent == repl->current) {
        res_t result = ik_repl_render_frame(repl);
        if (is_err(&result)) PANIC("render failed"); // LCOV_EXCL_BR_LINE
    }
}

void ik_repl_handle_tool_completion(ik_repl_ctx_t *repl)
{
    ik_repl_handle_agent_tool_completion(repl, repl->current);
}

res_t ik_repl_calculate_curl_min_timeout(ik_repl_ctx_t *repl, long *timeout_out)
{
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE
    assert(timeout_out != NULL);  // LCOV_EXCL_BR_LINE

    long curl_timeout_ms = -1;
    for (size_t i = 0; i < repl->agent_count; i++) {
        long agent_timeout = -1;
        CHECK(ik_openai_multi_timeout(repl->agents[i]->multi, &agent_timeout));
        if (agent_timeout >= 0) {
            if (curl_timeout_ms < 0 || agent_timeout < curl_timeout_ms) {  // LCOV_EXCL_BR_LINE
                curl_timeout_ms = agent_timeout;
            }
        }
    }
    *timeout_out = curl_timeout_ms;
    return OK(NULL);
}

res_t ik_repl_handle_select_timeout(ik_repl_ctx_t *repl)
{
    // Advance spinner if visible
    if (repl->current->spinner_state.visible) {
        ik_spinner_advance(&repl->current->spinner_state);
        CHECK(ik_repl_render_frame(repl));
    }

    // Check scroll detector timeout
    if (repl->scroll_det != NULL) {  // LCOV_EXCL_BR_LINE
        struct timespec ts;  // LCOV_EXCL_LINE
        clock_gettime(CLOCK_MONOTONIC, &ts);  // LCOV_EXCL_LINE
        int64_t now_ms = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;  // LCOV_EXCL_LINE
        ik_scroll_result_t timeout_result = ik_scroll_detector_check_timeout(  // LCOV_EXCL_LINE
            repl->scroll_det, now_ms);  // LCOV_EXCL_LINE

        // Process any flushed arrow - call handlers directly to bypass detector
        if (timeout_result == IK_SCROLL_RESULT_ARROW_UP) {  // LCOV_EXCL_BR_LINE LCOV_EXCL_LINE
            CHECK(ik_repl_handle_arrow_up_action(repl));  // LCOV_EXCL_LINE
            CHECK(ik_repl_render_frame(repl));  // LCOV_EXCL_LINE
        } else if (timeout_result == IK_SCROLL_RESULT_ARROW_DOWN) {  // LCOV_EXCL_BR_LINE LCOV_EXCL_LINE
            CHECK(ik_repl_handle_arrow_down_action(repl));  // LCOV_EXCL_LINE
            CHECK(ik_repl_render_frame(repl));  // LCOV_EXCL_LINE
        }
    }

    return OK(NULL);
}

res_t ik_repl_poll_tool_completions(ik_repl_ctx_t *repl)
{
    // Poll for tool thread completion - check ALL agents
    if (repl->agent_count > 0) {
        // Multi-agent mode: check all agents
        for (size_t i = 0; i < repl->agent_count; i++) {
            ik_agent_ctx_t *agent = repl->agents[i];

            pthread_mutex_lock_(&agent->tool_thread_mutex);
            ik_agent_state_t state = agent->state;
            bool complete = agent->tool_thread_complete;
            pthread_mutex_unlock_(&agent->tool_thread_mutex);

            if (state == IK_AGENT_STATE_EXECUTING_TOOL && complete) {
                ik_repl_handle_agent_tool_completion(repl, agent);
            }
        }
    } else if (repl->current != NULL) {
        // Single-agent mode (tests/legacy): check current only
        pthread_mutex_lock_(&repl->current->tool_thread_mutex);
        ik_agent_state_t state = repl->current->state;
        bool complete = repl->current->tool_thread_complete;
        pthread_mutex_unlock_(&repl->current->tool_thread_mutex);

        if (state == IK_AGENT_STATE_EXECUTING_TOOL && complete) {
            ik_repl_handle_agent_tool_completion(repl, repl->current);
        }
    }

    return OK(NULL);
}
