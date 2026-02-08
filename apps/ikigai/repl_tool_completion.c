#include "apps/ikigai/repl_tool_completion.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/event_render.h"
#include "shared/panic.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/providers/request.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/repl_callbacks.h"
#include "apps/ikigai/repl_event_handlers.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/shared.h"
#include "shared/wrapper.h"
#include "apps/ikigai/wrapper_pthread.h"
#include "shared/wrapper_internal.h"

#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <talloc.h>


#include "shared/poison.h"
void ik_repl_handle_agent_tool_completion(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent)
{
    if (agent->pending_tool_call != NULL) {
        ik_agent_complete_tool_execution(agent);
    } else {
        // Deferred command (e.g. /wait) - no pending_tool_call, just cleanup thread
        pthread_join_(agent->tool_thread, NULL);
        pthread_mutex_lock_(&agent->tool_thread_mutex);
        agent->tool_thread_running = false;
        agent->tool_thread_complete = false;
        agent->tool_thread_result = NULL;
        pthread_mutex_unlock_(&agent->tool_thread_mutex);
        agent->tool_child_pid = 0;
        ik_agent_transition_from_executing_tool(agent);
    }

    // Call on_complete hook if set (runs on main thread)
    if (agent->pending_on_complete != NULL) {
        agent->pending_on_complete(repl, agent);
        agent->pending_on_complete = NULL;
        agent->tool_deferred_data = NULL;
        // Free tool_thread_ctx now that on_complete has stolen what it needs
        if (agent->tool_thread_ctx != NULL) {
            talloc_free(agent->tool_thread_ctx);
            agent->tool_thread_ctx = NULL;
        }
    }

    if (ik_agent_should_continue_tool_loop(agent)) {
        agent->tool_iteration_count++;
        ik_repl_submit_tool_loop_continuation(repl, agent);
    } else {
        ik_agent_transition_to_idle(agent);
    }
    if (agent == repl->current) {
        res_t result = ik_repl_render_frame_(repl);
        if (is_err(&result)) PANIC("render failed"); // LCOV_EXCL_BR_LINE
    }
}

void ik_repl_handle_interrupted_tool_completion(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent)
{
    agent->interrupt_requested = false;
    pthread_join_(agent->tool_thread, NULL);

    // Deferred command (e.g. /wait) - minimal cleanup, preserve scrollback
    if (agent->pending_tool_call == NULL) {
        pthread_mutex_lock_(&agent->tool_thread_mutex);
        agent->tool_thread_running = false;
        agent->tool_thread_complete = false;
        agent->tool_thread_result = NULL;
        pthread_mutex_unlock_(&agent->tool_thread_mutex);
        agent->tool_child_pid = 0;
        ik_agent_transition_from_executing_tool(agent);

        if (agent->pending_on_complete != NULL) {
            agent->pending_on_complete(repl, agent);
            agent->pending_on_complete = NULL;
            agent->tool_deferred_data = NULL;
            agent->tool_thread_ctx = NULL;
        }

        ik_agent_transition_to_idle_(agent);
        if (agent == repl->current) {
            res_t result = ik_repl_render_frame_(repl);
            if (is_err(&result)) PANIC("render failed"); // LCOV_EXCL_BR_LINE
        }
        return;
    }

    // Standard tool call interruption - clear and re-render scrollback
    if (agent->tool_thread_ctx != NULL) {
        talloc_free(agent->tool_thread_ctx);
        agent->tool_thread_ctx = NULL;
    }
    talloc_free(agent->pending_tool_call);
    agent->pending_tool_call = NULL;
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    agent->tool_thread_running = false;
    agent->tool_thread_complete = false;
    agent->tool_thread_result = NULL;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);
    agent->tool_child_pid = 0;
    ik_agent_transition_from_executing_tool(agent);

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

    // Mark all messages from the interrupted turn as interrupted (don't remove)
    if (found_user && turn_start < agent->message_count) {
        for (size_t i = turn_start; i < agent->message_count; i++) {
            if (agent->messages[i] != NULL) {
                agent->messages[i]->interrupted = true;
            }
        }
    }

    // Clear scrollback and re-render all messages with interrupted styling
    ik_scrollback_clear(agent->scrollback);
    for (size_t i = 0; i < agent->message_count; i++) {
        ik_message_t *m = agent->messages[i];
        if (m == NULL || m->content_count == 0) continue;

        // Render first content block (simplified for interrupt recovery)
        ik_content_block_t *block = &m->content_blocks[0];
        const char *kind = NULL;
        const char *content = NULL;

        switch (m->role) {
            case IK_ROLE_USER:
                kind = "user";
                if (block->type == IK_CONTENT_TEXT) content = block->data.text.text;
                break;
            case IK_ROLE_ASSISTANT:
                kind = "assistant";
                if (block->type == IK_CONTENT_TEXT) content = block->data.text.text;
                break;
            case IK_ROLE_TOOL:
                kind = "tool_result";
                if (block->type == IK_CONTENT_TOOL_RESULT) content = block->data.tool_result.content;
                break;
        }

        if (kind != NULL && content != NULL) {
            ik_event_render(agent->scrollback, kind, content, "{}", m->interrupted);
        }
    }

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

void ik_repl_submit_tool_loop_continuation(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent)
{
    (void)repl; // Unused - was used for repl->shared->cfg

    // Get or create provider (lazy initialization)
    ik_provider_t *provider = NULL;
    res_t result = ik_agent_get_provider_(agent, (void **)&provider);
    if (is_err(&result)) {  // LCOV_EXCL_BR_LINE
        const char *err_msg = error_message(result.err);  // LCOV_EXCL_LINE
        ik_scrollback_append_line(agent->scrollback, err_msg, strlen(err_msg));  // LCOV_EXCL_LINE
        ik_agent_transition_to_idle(agent);  // LCOV_EXCL_LINE
        talloc_free(result.err);  // LCOV_EXCL_LINE
        return;  // LCOV_EXCL_LINE
    }

    // Build normalized request from conversation
    ik_request_t *req = NULL;
    result = ik_request_build_from_conversation_(agent, agent, agent->shared->tool_registry, (void **)&req);
    if (is_err(&result)) {  // LCOV_EXCL_BR_LINE
        const char *err_msg = error_message(result.err);  // LCOV_EXCL_LINE
        ik_scrollback_append_line(agent->scrollback, err_msg, strlen(err_msg));  // LCOV_EXCL_LINE
        ik_agent_transition_to_idle(agent);  // LCOV_EXCL_LINE
        talloc_free(result.err);  // LCOV_EXCL_LINE
        return;  // LCOV_EXCL_LINE
    }

    // Start async stream (returns immediately)
    result = provider->vt->start_stream(provider->ctx, req,
                                        ik_repl_stream_callback, agent,
                                        ik_repl_completion_callback, agent);
    if (is_err(&result)) {  // LCOV_EXCL_BR_LINE
        const char *err_msg = error_message(result.err);  // LCOV_EXCL_LINE
        ik_scrollback_append_line(agent->scrollback, err_msg, strlen(err_msg));  // LCOV_EXCL_LINE
        ik_agent_transition_to_idle(agent);  // LCOV_EXCL_LINE
        talloc_free(result.err);  // LCOV_EXCL_LINE
    } else {
        agent->curl_still_running = 1;
    }
}

res_t ik_repl_poll_tool_completions(ik_repl_ctx_t *repl)
{
    if (repl->agent_count > 0) {
        for (size_t i = 0; i < repl->agent_count; i++) {
            ik_agent_ctx_t *agent = repl->agents[i];
            pthread_mutex_lock_(&agent->tool_thread_mutex);
            ik_agent_state_t state = atomic_load(&agent->state);
            bool complete = agent->tool_thread_complete;
            pthread_mutex_unlock_(&agent->tool_thread_mutex);
            if (state == IK_AGENT_STATE_EXECUTING_TOOL && complete) {
                // Check interrupt flag before processing completion
                if (agent->interrupt_requested) {
                    ik_repl_handle_interrupted_tool_completion(repl, agent);
                } else {
                    ik_repl_handle_agent_tool_completion(repl, agent);
                }
            }
        }
    } else if (repl->current != NULL) {
        pthread_mutex_lock_(&repl->current->tool_thread_mutex);
        ik_agent_state_t state = atomic_load(&repl->current->state);
        bool complete = repl->current->tool_thread_complete;
        pthread_mutex_unlock_(&repl->current->tool_thread_mutex);
        if (state == IK_AGENT_STATE_EXECUTING_TOOL && complete) {
            // Check interrupt flag before processing completion
            if (repl->current->interrupt_requested) {
                ik_repl_handle_interrupted_tool_completion(repl, repl->current);
            } else {
                ik_repl_handle_agent_tool_completion(repl, repl->current);
            }
        }
    }
    return OK(NULL);
}
