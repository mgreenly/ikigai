#include "apps/ikigai/repl_tool_completion.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/db/message.h"
#include "apps/ikigai/event_render.h"
#include "apps/ikigai/format.h"
#include "apps/ikigai/message.h"
#include "shared/panic.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/providers/request.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/repl_callbacks.h"
#include "apps/ikigai/repl_event_handlers.h"
#include "apps/ikigai/repl_response_helpers.h"
#include "apps/ikigai/repl_tool_json.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/tool_scheduler.h"
#include "shared/wrapper.h"
#include "apps/ikigai/wrapper_pthread.h"
#include "shared/wrapper_internal.h"

#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <talloc.h>

#include "shared/poison.h"

ik_message_t *ik_repl_build_multi_tool_call_msg(ik_agent_ctx_t *agent,
                                                ik_tool_scheduler_t *sched)
{
    ik_message_t *msg = talloc_zero(agent, ik_message_t);
    if (!msg) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    msg->role = IK_ROLE_ASSISTANT;
    msg->kind = talloc_strdup(msg, "tool_call");
    if (!msg->kind) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    msg->content_blocks = talloc_zero_array(msg, ik_content_block_t,
                                            (unsigned int)sched->count);
    if (!msg->content_blocks) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    msg->content_count = (size_t)sched->count;

    for (int32_t i = 0; i < sched->count; i++) {
        ik_schedule_entry_t *e = &sched->entries[i];
        ik_content_block_t  *b = &msg->content_blocks[i];
        b->type = IK_CONTENT_TOOL_CALL;
        b->data.tool_call.id = talloc_strdup(msg, e->tool_call->id);
        b->data.tool_call.name = talloc_strdup(msg, e->tool_call->name);
        b->data.tool_call.arguments = talloc_strdup(msg, e->tool_call->arguments);
        b->data.tool_call.thought_signature = e->thought_signature != NULL
            ? talloc_strdup(msg, e->thought_signature) : NULL;
        if (!b->data.tool_call.id || !b->data.tool_call.name ||
            !b->data.tool_call.arguments) {
            PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        }
    }
    return msg;
}

static const char *entry_result_str(ik_agent_ctx_t *agent,
                                    ik_tool_scheduler_t *sched,
                                    int32_t idx,
                                    bool *is_error_out)
{
    ik_schedule_entry_t *e = &sched->entries[idx];
    if (e->status == IK_SCHEDULE_COMPLETED) {
        *is_error_out = false;
        return e->result != NULL ? e->result : "{}";
    }
    if (e->status == IK_SCHEDULE_ERRORED) {
        *is_error_out = true;
        return e->error != NULL ? e->error : "tool execution failed";
    }
    *is_error_out = true;
    const char *blocker = "prerequisite";
    if (e->blocked_by_count > 0) {
        blocker = sched->entries[e->blocked_by[0]].tool_call->name;
    }
    return talloc_asprintf(agent, "Skipped: prerequisite tool '%s' failed", blocker);
}

ik_message_t *ik_repl_build_multi_tool_result_msg(ik_agent_ctx_t *agent,
                                                   ik_tool_scheduler_t *sched)
{
    ik_message_t *msg = talloc_zero(agent, ik_message_t);
    if (!msg) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    msg->role = IK_ROLE_TOOL;
    msg->kind = talloc_strdup(msg, "tool_result");
    if (!msg->kind) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    msg->content_blocks = talloc_zero_array(msg, ik_content_block_t,
                                            (unsigned int)sched->count);
    if (!msg->content_blocks) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    msg->content_count = (size_t)sched->count;

    for (int32_t i = 0; i < sched->count; i++) {
        ik_schedule_entry_t *e  = &sched->entries[i];
        ik_content_block_t  *b  = &msg->content_blocks[i];
        bool is_error = false;
        const char *result = entry_result_str(agent, sched, i, &is_error);
        b->type = IK_CONTENT_TOOL_RESULT;
        b->data.tool_result.tool_call_id = talloc_strdup(msg, e->tool_call->id);
        b->data.tool_result.content      = talloc_strdup(msg, result);
        b->data.tool_result.is_error     = is_error;
        if (!b->data.tool_result.tool_call_id || !b->data.tool_result.content) {
            PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        }
    }
    return msg;
}

static void persist_entry(ik_agent_ctx_t *agent, ik_tool_scheduler_t *sched,
                           int32_t idx, const char *batch_id)
{
    ik_schedule_entry_t *e       = &sched->entries[idx];
    ik_tool_call_t      *tc      = e->tool_call;
    bool is_error                = false;
    const char *result_json      = entry_result_str(agent, sched, idx, &is_error);
    // Scrollback display is handled live by the scheduler (display_tool_input/output).
    // Only persist to DB here.
    if (agent->shared->db_ctx == NULL || agent->shared->session_id <= 0) return;
    const char *formatted_call   = ik_format_tool_call(agent, tc);
    const char *formatted_result = ik_format_tool_result(agent, tc->name, result_json);
    char *call_data   = ik_build_tool_call_data_json(agent, tc, NULL, NULL, NULL, batch_id, e->thought_signature);
    char *result_data = ik_build_tool_result_data_json(agent, tc->id, tc->name,
                                                       result_json);
    ik_db_message_insert_(agent->shared->db_ctx, agent->shared->session_id,
                          agent->uuid, "tool_call", formatted_call, call_data);
    ik_db_message_insert_(agent->shared->db_ctx, agent->shared->session_id,
                          agent->uuid, "tool_result", formatted_result, result_data);
    talloc_free(call_data);
    talloc_free(result_data);
}

static void ik_repl_complete_scheduler(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent)
{
    ik_tool_scheduler_t *sched = agent->scheduler;
    ik_message_t *call_msg   = ik_repl_build_multi_tool_call_msg(agent, sched);
    ik_message_t *result_msg = ik_repl_build_multi_tool_result_msg(agent, sched);
    res_t r = ik_agent_add_message(agent, call_msg);
    if (is_err(&r)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE
    r = ik_agent_add_message(agent, result_msg);
    if (is_err(&r)) PANIC("allocation failed"); // LCOV_EXCL_BR_LINE
    const char *batch_id = (sched->count > 0) ? sched->entries[0].tool_call->id : NULL;
    for (int32_t i = 0; i < sched->count; i++) {
        persist_entry(agent, sched, i, batch_id);
    }
    ik_tool_scheduler_destroy(sched);
    agent->scheduler = NULL;
    ik_agent_transition_from_executing_tool(agent);
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

static void poll_agent_scheduler(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent)
{
    ik_tool_scheduler_poll(agent->scheduler);
    if (ik_tool_scheduler_all_terminal(agent->scheduler)) {
        ik_repl_complete_scheduler(repl, agent);
    }
}

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
    ik_repl_render_usage_event(agent);

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

static void poll_one_agent(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent)
{
    ik_agent_state_t state = atomic_load(&agent->state);
    if (state != IK_AGENT_STATE_EXECUTING_TOOL) return;

    // Scheduler path: poll all entries, complete when all terminal
    if (agent->scheduler != NULL) {
        poll_agent_scheduler(repl, agent);
        return;
    }

    // Single-tool path: check mutex-guarded completion flag
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    bool complete = agent->tool_thread_complete;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);
    if (!complete) return;

    if (agent->interrupt_requested) {
        ik_repl_handle_interrupted_tool_completion(repl, agent);
    } else {
        ik_repl_handle_agent_tool_completion(repl, agent);
    }
}

res_t ik_repl_poll_tool_completions(ik_repl_ctx_t *repl)
{
    if (repl->agent_count > 0) {
        for (size_t i = 0; i < repl->agent_count; i++) {
            poll_one_agent(repl, repl->agents[i]);
        }
    } else if (repl->current != NULL) {
        poll_one_agent(repl, repl->current);
    }
    return OK(NULL);
}
