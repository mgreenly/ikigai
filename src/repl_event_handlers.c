#include "repl_event_handlers.h"

#include "agent.h"

#include <errno.h>
#include <string.h>
#include "db/message.h"
#include "event_render.h"
#include "input.h"
#include "layer_wrappers.h"
#include "logger.h"
#include "message.h"
#include "panic.h"
#include "providers/provider.h"
#include "providers/request.h"
#include "repl.h"
#include "repl_actions.h"
#include "repl_actions_internal.h"
#include "repl_callbacks.h"
#include "repl_tool_completion.h"
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
static void persist_assistant_msg(ik_repl_ctx_t *repl);

long ik_repl_calculate_select_timeout_ms(ik_repl_ctx_t *repl, long curl_timeout_ms)
{
    long spinner_timeout_ms = repl->current->spinner_state.visible ? 80 : -1;  // LCOV_EXCL_BR_LINE
    long tool_poll_timeout_ms = -1;
    for (size_t i = 0; i < repl->agent_count; i++) {
        ik_agent_ctx_t *agent = repl->agents[i];
        pthread_mutex_lock_(&agent->tool_thread_mutex);
        bool executing = (agent->state == IK_AGENT_STATE_EXECUTING_TOOL);
        pthread_mutex_unlock_(&agent->tool_thread_mutex);
        if (executing) {
            tool_poll_timeout_ms = 50;
            break;
        }
    }

    long scroll_timeout_ms = -1;
    if (repl->scroll_det != NULL) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        int64_t now_ms = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
        scroll_timeout_ms = ik_scroll_detector_get_timeout_ms(repl->scroll_det, now_ms);
    }

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
    int32_t terminal_fd = repl->shared->term->tty_fd;
    FD_SET(terminal_fd, read_fds);
    int max_fd = terminal_fd;
    for (size_t i = 0; i < repl->agent_count; i++) {
        ik_agent_ctx_t *agent = repl->agents[i];
        if (agent->provider_instance != NULL) {
            int agent_max_fd = -1;
            res_t result = agent->provider_instance->vt->fdset(agent->provider_instance->ctx,
                                                               read_fds, write_fds, exc_fds, &agent_max_fd);
            if (is_err(&result)) return result;
            if (agent_max_fd > max_fd) {
                max_fd = agent_max_fd;
            }
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
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    repl->render_start_us = (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
    ik_input_action_t action;
    ik_input_parse_byte(repl->input_parser, byte, &action);
    res_t result = ik_repl_process_action(repl, &action);
    if (is_err(&result)) return result; // LCOV_EXCL_BR_LINE
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

    // Store provider information
    if (repl->current->provider != NULL) {
        data_json = talloc_asprintf_append(data_json, "\"provider\":\"%s\"", repl->current->provider);
        first = false;
    }

    // Store model information
    if (repl->current->response_model != NULL) {
        data_json = talloc_asprintf_append(data_json, "%s\"model\":\"%s\"",
                                           first ? "" : ",", repl->current->response_model);
        first = false;
    }

    // Store thinking level if set (not NONE)
    if (repl->current->thinking_level > 0) {
        const char *level_str = "unknown";
        switch (repl->current->thinking_level) {
            case 1: level_str = "low"; break;
            case 2: level_str = "med"; break;
            case 3: level_str = "high"; break;
        }
        data_json = talloc_asprintf_append(data_json, "%s\"thinking_level\":\"%s\"",
                                           first ? "" : ",", level_str);
        first = false;
    }

    // Store finish reason
    if (repl->current->response_finish_reason != NULL) {
        data_json = talloc_asprintf_append(data_json, "%s\"finish_reason\":\"%s\"",
                                           first ? "" : ",", repl->current->response_finish_reason);
    }

    data_json = talloc_strdup_append(data_json, "}");

    res_t db_res = ik_db_message_insert_(repl->shared->db_ctx, repl->shared->session_id,
                                         repl->current->uuid, "assistant", repl->current->assistant_response,
                                         data_json);
    if (is_err(&db_res)) {  // LCOV_EXCL_BR_LINE
        talloc_free(db_res.err);  // LCOV_EXCL_LINE
    }
    talloc_free(data_json);

    // Persist usage event separately (for token display on replay)
    int32_t total = repl->current->response_input_tokens +
                    repl->current->response_output_tokens +
                    repl->current->response_thinking_tokens;
    if (total > 0) {
        char *usage_json = talloc_asprintf(repl,
                                           "{\"input_tokens\":%d,\"output_tokens\":%d,\"thinking_tokens\":%d}",
                                           repl->current->response_input_tokens,
                                           repl->current->response_output_tokens,
                                           repl->current->response_thinking_tokens);
        db_res = ik_db_message_insert_(repl->shared->db_ctx, repl->shared->session_id,
                                       repl->current->uuid, "usage", NULL, usage_json);
        if (is_err(&db_res)) {  // LCOV_EXCL_BR_LINE
            talloc_free(db_res.err);  // LCOV_EXCL_LINE
        }
        talloc_free(usage_json);
    }
}

static void handle_agent_request_error(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent)
{
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
    talloc_free(agent->http_error_message);
    agent->http_error_message = NULL;
    if (agent->assistant_response != NULL) {
        talloc_free(agent->assistant_response);
        agent->assistant_response = NULL;
    }
}

void ik_repl_handle_agent_request_success(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent)
{
    if (agent->assistant_response != NULL && strlen(agent->assistant_response) > 0) {
        ik_message_t *assistant_msg = ik_message_create_text(agent, IK_ROLE_ASSISTANT, agent->assistant_response);
        res_t result = ik_agent_add_message_(agent, assistant_msg);
        if (is_err(&result)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE
        persist_assistant_msg(repl);
    }
    if (agent->assistant_response != NULL) {
        talloc_free(agent->assistant_response);
        agent->assistant_response = NULL;
    }
    if (agent->pending_tool_call != NULL) {
        ik_agent_start_tool_execution_(agent);
        return;
    }
    if (ik_agent_should_continue_tool_loop_(agent)) {
        agent->tool_iteration_count++;
        ik_repl_submit_tool_loop_continuation_(repl, agent);
    }
}

void ik_repl_handle_interrupted_llm_completion(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent)
{
    agent->interrupt_requested = false;
    if (agent->http_error_message != NULL) {
        talloc_free(agent->http_error_message);
        agent->http_error_message = NULL;
    }
    if (agent->assistant_response != NULL) {
        talloc_free(agent->assistant_response);
        agent->assistant_response = NULL;
    }

    // Find the most recent user message (start of the interrupted turn)
    size_t turn_start = 0;
    bool found_user = false;
    for (size_t i = agent->message_count; i > 0; i--) {
        ik_message_t *m = agent->messages[i - 1];
        if (m != NULL && m->role == IK_ROLE_USER) {
            turn_start = i - 1;
            found_user = true;
            break;
        }
    }

    // Remove all messages from the interrupted turn (in-memory)
    if (found_user && turn_start < agent->message_count) {
        for (size_t i = turn_start; i < agent->message_count; i++) {
            if (agent->messages[i] != NULL) {
                talloc_free(agent->messages[i]);
                agent->messages[i] = NULL;
            }
        }
        agent->message_count = turn_start;
    }

    const char *msg = "Interrupted";
    ik_scrollback_append_line(agent->scrollback, msg, strlen(msg));
    ik_scrollback_append_line(agent->scrollback, "", 0);
    if (repl->shared->db_ctx != NULL && repl->shared->session_id > 0) {
        res_t db_res = ik_db_message_insert_(repl->shared->db_ctx, repl->shared->session_id,
                                             agent->uuid, "interrupted", NULL, NULL);
        if (is_err(&db_res)) {  // LCOV_EXCL_BR_LINE
            talloc_free(db_res.err);  // LCOV_EXCL_LINE
        }
    }
    ik_agent_transition_to_idle_(agent);
    if (agent == repl->current) {
        res_t result = ik_repl_render_frame_(repl);
        if (is_err(&result)) PANIC("render failed"); // LCOV_EXCL_BR_LINE
    }
}

static res_t process_agent_curl_events(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent)
{
    if (agent->curl_still_running > 0 && agent->provider_instance != NULL) {
        int prev_running = agent->curl_still_running;
        CHECK(agent->provider_instance->vt->perform(agent->provider_instance->ctx, &agent->curl_still_running));
        agent->provider_instance->vt->info_read(agent->provider_instance->ctx, repl->shared->logger);
        pthread_mutex_lock_(&agent->tool_thread_mutex);
        ik_agent_state_t current_state = agent->state;
        pthread_mutex_unlock_(&agent->tool_thread_mutex);
        if (prev_running > 0 && agent->curl_still_running == 0 && current_state == IK_AGENT_STATE_WAITING_FOR_LLM) {  // LCOV_EXCL_BR_LINE
            // Check interrupt flag before processing completion
            if (agent->interrupt_requested) {
                ik_repl_handle_interrupted_llm_completion(repl, agent);
            } else {
                if (agent->http_error_message != NULL) {
                    handle_agent_request_error(repl, agent);
                } else {
                    ik_repl_handle_agent_request_success(repl, agent);
                }
                pthread_mutex_lock_(&agent->tool_thread_mutex);
                current_state = agent->state;
                pthread_mutex_unlock_(&agent->tool_thread_mutex);
                if (current_state == IK_AGENT_STATE_WAITING_FOR_LLM) {
                    ik_agent_transition_to_idle_(agent);
                }
                if (agent == repl->current) {
                    CHECK(ik_repl_render_frame(repl)); // LCOV_EXCL_BR_LINE - render only fails on terminal write error
                }
            }
        }
    }
    return OK(NULL);
}

res_t ik_repl_handle_curl_events(ik_repl_ctx_t *repl, int ready)
{
    (void)ready;
    for (size_t i = 0; i < repl->agent_count; i++) {
        ik_agent_ctx_t *agent = repl->agents[i];
        CHECK(process_agent_curl_events(repl, agent));
    }
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

res_t ik_repl_calculate_curl_min_timeout(ik_repl_ctx_t *repl, long *timeout_out)
{
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE
    assert(timeout_out != NULL);  // LCOV_EXCL_BR_LINE

    long curl_timeout_ms = -1;
    for (size_t i = 0; i < repl->agent_count; i++) {
        if (repl->agents[i]->provider_instance != NULL) {
            long agent_timeout = -1;
            CHECK(repl->agents[i]->provider_instance->vt->timeout(repl->agents[i]->provider_instance->ctx,
                                                                  &agent_timeout));
            if (agent_timeout >= 0) {
                if (curl_timeout_ms < 0 || agent_timeout < curl_timeout_ms) {  // LCOV_EXCL_BR_LINE
                    curl_timeout_ms = agent_timeout;
                }
            }
        }
    }
    *timeout_out = curl_timeout_ms;
    return OK(NULL);
}

res_t ik_repl_handle_select_timeout(ik_repl_ctx_t *repl)
{
    if (repl->current->spinner_state.visible) {
        ik_spinner_advance(&repl->current->spinner_state);
        CHECK(ik_repl_render_frame(repl));
    }
    if (repl->scroll_det != NULL) {  // LCOV_EXCL_BR_LINE
        struct timespec ts;  // LCOV_EXCL_LINE
        clock_gettime(CLOCK_MONOTONIC, &ts);  // LCOV_EXCL_LINE
        int64_t now_ms = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;  // LCOV_EXCL_LINE
        ik_scroll_result_t timeout_result = ik_scroll_detector_check_timeout(  // LCOV_EXCL_LINE
            repl->scroll_det, now_ms);  // LCOV_EXCL_LINE
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
