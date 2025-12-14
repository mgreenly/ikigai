#include "repl.h"
#include "agent.h"

#include "event_render.h"
#include "input_buffer/core.h"
#include "logger.h"
#include "openai/client_multi.h"
#include "panic.h"
#include "render_cursor.h"
#include "repl_actions.h"
#include "repl_actions_internal.h"
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
        CHECK(ik_openai_multi_timeout(repl->current->multi, &curl_timeout_ms));
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

        // Handle timeout (spinner animation and scroll detector)
        // Note: Don't continue here - curl events must still be processed
        if (ready == 0) {
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
        }

        // Handle debug pipes
        if (ready > 0 && repl->shared->debug_mgr != NULL) {  // LCOV_EXCL_BR_LINE
            ik_debug_mgr_handle_ready(repl->shared->debug_mgr, &read_fds, repl->current->scrollback, repl->shared->debug_enabled);  // LCOV_EXCL_LINE
        }

        // Handle terminal input
        if (FD_ISSET(repl->shared->term->tty_fd, &read_fds)) {  // LCOV_EXCL_BR_LINE
            CHECK(handle_terminal_input(repl, repl->shared->term->tty_fd, &should_exit));
            if (should_exit) break;
        }

        // Handle curl_multi events
        CHECK(handle_curl_events(repl, ready));  // LCOV_EXCL_BR_LINE

        // Poll for tool thread completion
        pthread_mutex_lock_(&repl->current->tool_thread_mutex);
        ik_agent_state_t current_state = repl->current->state;
        bool complete = repl->current->tool_thread_complete;
        pthread_mutex_unlock_(&repl->current->tool_thread_mutex);

        if (current_state == IK_AGENT_STATE_EXECUTING_TOOL && complete) {
            handle_tool_completion(repl);
        }
    }

    return OK(NULL);
}

res_t ik_repl_submit_line(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */

    // Get current input buffer text
    const uint8_t *text_data = repl->current->input_buffer->text->data;
    size_t text_len = ik_byte_array_size(repl->current->input_buffer->text);

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
            yyjson_mut_doc *log_doc = ik_log_create();  // LCOV_EXCL_LINE
            yyjson_mut_val *root = yyjson_mut_doc_get_root(log_doc);  // LCOV_EXCL_LINE
            yyjson_mut_obj_add_str(log_doc, root, "message", "Failed to append to history file");  // LCOV_EXCL_LINE
            yyjson_mut_obj_add_str(log_doc, root, "error", result.err->msg);  // LCOV_EXCL_LINE
            ik_logger_warn_json(repl->shared->logger, log_doc);  // LCOV_EXCL_LINE
            talloc_free(result.err);  // LCOV_EXCL_LINE
        }

        talloc_free(text);

        // Exit browsing mode if active
        if (ik_history_is_browsing(repl->shared->history)) {  // LCOV_EXCL_BR_LINE
            ik_history_stop_browsing(repl->shared->history);  // LCOV_EXCL_LINE
        }
    }

    // Render user message via event renderer
    if (text_len > 0 && repl->current->scrollback != NULL) {  // LCOV_EXCL_BR_LINE
        char *text = talloc_size(NULL, text_len + 1);
        if (text == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        memcpy(text, text_data, text_len);
        text[text_len] = '\0';
        res_t result = ik_event_render(repl->current->scrollback, "user", text, "{}");
        talloc_free(text);
        if (is_err(&result)) return result;
    }

    ik_input_buffer_clear(repl->current->input_buffer);
    repl->current->viewport_offset = 0; // Auto-scroll to bottom

    return OK(NULL);
}

res_t ik_repl_handle_resize(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->shared->term != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->current->scrollback != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->current->input_buffer != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(repl->shared->render != NULL);   /* LCOV_EXCL_BR_LINE */

    int rows, cols;
    res_t result = ik_term_get_size(repl->shared->term, &rows, &cols);
    if (is_err(&result)) return result;

    repl->shared->render->rows = rows;
    repl->shared->render->cols = cols;

    ik_scrollback_ensure_layout(repl->current->scrollback, cols);
    ik_input_buffer_ensure_layout(repl->current->input_buffer, cols);

    // Trigger immediate redraw with new dimensions
    return ik_repl_render_frame(repl);
}

void ik_repl_transition_to_waiting_for_llm(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */

    // Update state with mutex protection for thread safety
    pthread_mutex_lock_(&repl->current->tool_thread_mutex);
    assert(repl->current->state == IK_AGENT_STATE_IDLE);   /* LCOV_EXCL_BR_LINE */
    repl->current->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    pthread_mutex_unlock_(&repl->current->tool_thread_mutex);

    // Show spinner, hide input
    repl->current->spinner_state.visible = true;
    repl->current->input_buffer_visible = false;
}

void ik_repl_transition_to_idle(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */

    // Update state with mutex protection for thread safety
    pthread_mutex_lock_(&repl->current->tool_thread_mutex);
    assert(repl->current->state == IK_AGENT_STATE_WAITING_FOR_LLM);   /* LCOV_EXCL_BR_LINE */
    repl->current->state = IK_AGENT_STATE_IDLE;
    pthread_mutex_unlock_(&repl->current->tool_thread_mutex);

    // Hide spinner, show input
    repl->current->spinner_state.visible = false;
    repl->current->input_buffer_visible = true;
}

void ik_repl_transition_to_executing_tool(ik_repl_ctx_t *repl)
{
    assert(repl != NULL); /* LCOV_EXCL_BR_LINE */
    pthread_mutex_lock_(&repl->current->tool_thread_mutex);
    assert(repl->current->state == IK_AGENT_STATE_WAITING_FOR_LLM); /* LCOV_EXCL_BR_LINE */
    repl->current->state = IK_AGENT_STATE_EXECUTING_TOOL;
    pthread_mutex_unlock_(&repl->current->tool_thread_mutex);
}

void ik_repl_transition_from_executing_tool(ik_repl_ctx_t *repl)
{
    assert(repl != NULL); /* LCOV_EXCL_BR_LINE */
    pthread_mutex_lock_(&repl->current->tool_thread_mutex);
    assert(repl->current->state == IK_AGENT_STATE_EXECUTING_TOOL); /* LCOV_EXCL_BR_LINE */
    repl->current->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    pthread_mutex_unlock_(&repl->current->tool_thread_mutex);
}

bool ik_repl_should_continue_tool_loop(const ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */

    /* Check if finish_reason is "tool_calls" */
    if (repl->current->response_finish_reason == NULL) {
        return false;
    }

    if (strcmp(repl->current->response_finish_reason, "tool_calls") != 0) {
        return false;
    }

    /* Check if we've reached the tool iteration limit (if config is available) */
    if (repl->shared->cfg != NULL && repl->current->tool_iteration_count >= repl->shared->cfg->max_tool_turns) {
        return false;
    }

    return true;
}

res_t ik_repl_add_agent(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent)
{
    assert(repl != NULL);   // LCOV_EXCL_BR_LINE
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE

    // Grow array if needed
    if (repl->agent_count >= repl->agent_capacity) {
        size_t new_capacity = repl->agent_capacity == 0 ? 4 : repl->agent_capacity * 2;
        ik_agent_ctx_t **new_agents = talloc_realloc(repl, repl->agents, ik_agent_ctx_t *, (unsigned int)new_capacity);
        if (new_agents == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        repl->agents = new_agents;
        repl->agent_capacity = new_capacity;
    }

    // Add agent to array
    repl->agents[repl->agent_count] = agent;
    repl->agent_count++;

    return OK(NULL);
}

res_t ik_repl_remove_agent(ik_repl_ctx_t *repl, const char *uuid)
{
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE
    assert(uuid != NULL);  // LCOV_EXCL_BR_LINE

    // Find agent by UUID
    size_t index = 0;
    bool found = false;
    for (size_t i = 0; i < repl->agent_count; i++) {
        if (strcmp(repl->agents[i]->uuid, uuid) == 0) {
            index = i;
            found = true;
            break;
        }
    }

    if (!found) {
        return ERR(repl, AGENT_NOT_FOUND, "Agent not found: %s", uuid);
    }

    // Update current pointer if we're removing the current agent
    if (repl->agents[index] == repl->current) {
        repl->current = NULL;
    }

    // Shift remaining agents down
    for (size_t i = index; i < repl->agent_count - 1; i++) {
        repl->agents[i] = repl->agents[i + 1];
    }

    repl->agent_count--;

    return OK(NULL);
}

ik_agent_ctx_t *ik_repl_find_agent(ik_repl_ctx_t *repl, const char *uuid_prefix)
{
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE
    assert(uuid_prefix != NULL);  // LCOV_EXCL_BR_LINE

    // Minimum prefix length is 4 characters
    size_t prefix_len = strlen(uuid_prefix);
    if (prefix_len < 4) {
        return NULL;
    }

    // First pass: check for exact match (takes priority)
    for (size_t i = 0; i < repl->agent_count; i++) {
        if (strcmp(repl->agents[i]->uuid, uuid_prefix) == 0) {
            return repl->agents[i];
        }
    }

    // Second pass: check for prefix match
    ik_agent_ctx_t *match = NULL;
    size_t match_count = 0;

    for (size_t i = 0; i < repl->agent_count; i++) {
        if (strncmp(repl->agents[i]->uuid, uuid_prefix, prefix_len) == 0) {
            match = repl->agents[i];
            match_count++;
            if (match_count > 1) {
                // Ambiguous - multiple matches
                return NULL;
            }
        }
    }

    return match;
}

bool ik_repl_uuid_ambiguous(ik_repl_ctx_t *repl, const char *uuid_prefix)
{
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE
    assert(uuid_prefix != NULL);  // LCOV_EXCL_BR_LINE

    // Minimum prefix length is 4 characters
    size_t prefix_len = strlen(uuid_prefix);
    if (prefix_len < 4) {
        return false;
    }

    // Count matches
    size_t match_count = 0;

    for (size_t i = 0; i < repl->agent_count; i++) {
        if (strncmp(repl->agents[i]->uuid, uuid_prefix, prefix_len) == 0) {
            match_count++;
            if (match_count > 1) {
                return true;
            }
        }
    }

    return false;
}

res_t ik_repl_switch_agent(ik_repl_ctx_t *repl, ik_agent_ctx_t *new_agent)
{
    assert(repl != NULL);  // LCOV_EXCL_BR_LINE

    if (new_agent == NULL) {
        return ERR(repl, INVALID_ARG, "Cannot switch to NULL agent");
    }

    if (new_agent == repl->current) {
        return OK(NULL);  // No-op, already on this agent
    }

    // State is already stored on agent_ctx:
    // - repl->current->input_buffer (per-agent)
    // - repl->current->viewport_offset (per-agent)
    // These don't need explicit save/restore because
    // they're already per-agent fields.

    // Switch current pointer
    repl->current = new_agent;

    return OK(NULL);
}
