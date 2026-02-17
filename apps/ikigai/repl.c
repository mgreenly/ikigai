#include "apps/ikigai/repl.h"
#include "apps/ikigai/repl_internal.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/control_socket.h"
#include "apps/ikigai/event_render.h"
#include "apps/ikigai/history_io.h"
#include "apps/ikigai/input_buffer/core.h"
#include "apps/ikigai/layer_wrappers.h"
#include "shared/logger.h"
#include "shared/panic.h"
#include "apps/ikigai/render_cursor.h"
#include "apps/ikigai/repl_actions.h"
#include "apps/ikigai/repl_actions_internal.h"
#include "apps/ikigai/repl_event_handlers.h"
#include "apps/ikigai/repl_tool_completion.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/signal_handler.h"
#include "shared/wrapper.h"

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


#include "shared/poison.h"
void ik_repl_handle_control_socket_events(ik_repl_ctx_t *repl, fd_set *read_fds)
{
    if (repl->control_socket == NULL) {
        return;
    }
    if (ik_control_socket_listen_ready(repl->control_socket, read_fds)) {
        res_t accept_result = ik_control_socket_accept(repl->control_socket);
        if (is_err(&accept_result)) {
            talloc_free(accept_result.err);
        }
    }
    if (ik_control_socket_client_ready(repl->control_socket, read_fds)) {
        res_t client_result = ik_control_socket_handle_client(repl->control_socket, repl);
        if (is_err(&client_result)) {
            talloc_free(client_result.err);
        }
    }
}

res_t ik_repl_handle_key_injection(ik_repl_ctx_t *repl, bool *handled)
{
    *handled = false;
    if (repl->key_inject_buf == NULL || ik_key_inject_pending(repl->key_inject_buf) == 0) {
        return OK(NULL);
    }
    char byte;
    if (!ik_key_inject_drain(repl->key_inject_buf, &byte)) {
        return OK(NULL);
    }
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    repl->render_start_us = (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
    ik_input_action_t action;
    ik_input_parse_byte(repl->input_parser, byte, &action);
    CHECK(ik_repl_process_action(repl, &action));
    if (action.type != IK_INPUT_UNKNOWN) {
        CHECK(ik_repl_render_frame(repl));
    }
    *handled = true;
    return OK(NULL);
}

res_t ik_repl_run(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */

    // Initialize control socket
    if (repl->shared->paths != NULL) {
        res_t socket_result = ik_control_socket_init(repl, repl->shared->paths, &repl->control_socket);
        if (is_err(&socket_result)) {
            talloc_free(socket_result.err);
            repl->control_socket = NULL;
        }
    }

    // Initial render
    res_t result = ik_repl_render_frame(repl);
    if (is_err(&result)) {
        if (repl->control_socket != NULL) {
            ik_control_socket_destroy(repl->control_socket);
        }
        return result;
    }

    // Main event loop
    bool should_exit = false;
    while (!repl->quit && !should_exit) {  // LCOV_EXCL_BR_LINE
        // Check for pending resize
        CHECK(ik_signal_check_resize(repl));  // LCOV_EXCL_BR_LINE

        // Drain one byte from key injection buffer if available
        // This prevents interleaving injected and real input through the stateful parser
        bool handled = false;
        CHECK(ik_repl_handle_key_injection(repl, &handled));
        if (handled) {
            continue;  // Skip select() and tty read - drain buffer first
        }

        // Set up fd_sets
        fd_set read_fds, write_fds, exc_fds;
        int max_fd;
        CHECK(ik_repl_setup_fd_sets(repl, &read_fds, &write_fds, &exc_fds, &max_fd));  // LCOV_EXCL_BR_LINE

        // Calculate minimum curl timeout across ALL agents
        long curl_timeout_ms = -1;
        CHECK(ik_repl_calculate_curl_min_timeout(repl, &curl_timeout_ms));
        long effective_timeout_ms = ik_repl_calculate_select_timeout_ms(repl, curl_timeout_ms);

        struct timeval timeout;
        timeout.tv_sec = effective_timeout_ms / 1000;
        timeout.tv_usec = (effective_timeout_ms % 1000) * 1000;

        ik_repl_dev_dump_framebuffer(repl);

        // Call select()
        int ready = posix_select_(max_fd + 1, &read_fds, &write_fds, &exc_fds, &timeout);

        if (ready < 0) {
            if (errno == EINTR) {
                CHECK(ik_signal_check_resize(repl));  // LCOV_EXCL_BR_LINE
                continue;
            }
            break;
        }

        // Handle timeout (scroll detector only - spinner is now time-based)
        // Note: Don't continue here - curl events must still be processed
        if (ready == 0) {
            CHECK(ik_repl_handle_select_timeout(repl));  // LCOV_EXCL_BR_LINE
        }

        // Handle terminal input
        if (FD_ISSET(repl->shared->term->tty_fd, &read_fds)) {  // LCOV_EXCL_BR_LINE
            CHECK(ik_repl_handle_terminal_input(repl, repl->shared->term->tty_fd, &should_exit));
            if (should_exit) break;
        }

        ik_repl_handle_control_socket_events(repl, &read_fds);

        // Handle curl_multi events
        CHECK(ik_repl_handle_curl_events(repl, ready));  // LCOV_EXCL_BR_LINE

        // Time-based spinner advancement (independent of select timeout)
        if (repl->current->spinner_state.visible) {
            struct timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            int64_t now_ms = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
            if (ik_spinner_maybe_advance(&repl->current->spinner_state, now_ms)) {
                CHECK(ik_repl_render_frame(repl));
            }
        }

        // Poll for tool thread completion - check ALL agents
        CHECK(ik_repl_poll_tool_completions(repl));  // LCOV_EXCL_BR_LINE

        // Poll for pending prompts - check all agents for deferred fork prompts
        for (size_t i = 0; i < repl->agent_count; i++) {
            ik_agent_ctx_t *agent = repl->agents[i];
            if (agent->pending_prompt != NULL) {
                char *prompt = agent->pending_prompt;
                agent->pending_prompt = NULL;  // Clear before sending to prevent re-processing
                ik_event_render(agent->scrollback, "user", prompt, "{}", false);
                send_to_llm_for_agent(repl, agent, prompt);
                talloc_free(prompt);
            }
        }
    }

    // Destroy control socket
    if (repl->control_socket != NULL) {
        ik_control_socket_destroy(repl->control_socket);
        repl->control_socket = NULL;
    }

    return OK(NULL);
}

res_t ik_repl_submit_line(ik_repl_ctx_t *repl)
{
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */

    // Reject submission if current agent is dead
    if (repl->current->dead) {
        return OK(NULL);  // Silent rejection - dead agents cannot submit input
    }

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
        res_t result = ik_event_render(repl->current->scrollback, "user", text, "{}", false);
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

bool ik_agent_should_continue_tool_loop(const ik_agent_ctx_t *agent)
{
    assert(agent != NULL);   /* LCOV_EXCL_BR_LINE */

    /* Check if finish_reason is "tool_use" */
    if (agent->response_finish_reason == NULL) {
        return false;
    }

    if (strcmp(agent->response_finish_reason, "tool_use") != 0) {
        return false;
    }

    /* Check if we've reached the tool iteration limit (if config is available) */
    if (agent->repl->shared->cfg != NULL && agent->tool_iteration_count >= agent->repl->shared->cfg->max_tool_turns) {
        return false;
    }

    return true;
}
