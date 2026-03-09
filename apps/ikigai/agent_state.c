#include "apps/ikigai/agent.h"

#include "apps/ikigai/ansi.h"
#include "apps/ikigai/event_render.h"
#include "apps/ikigai/format.h"
#include "apps/ikigai/msg.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/repl_response_helpers.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/summary.h"
#include "apps/ikigai/summary_worker.h"
#include "apps/ikigai/token_cache.h"
#include "apps/ikigai/tool.h"
#include "apps/ikigai/wrapper_pthread.h"

#include "apps/ikigai/debug_log.h"

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "shared/poison.h"
bool ik_agent_has_running_tools(const ik_agent_ctx_t *agent)
{
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE
    return agent->tool_thread_running;
}

void ik_agent_transition_to_waiting_for_llm(ik_agent_ctx_t *agent)
{
    assert(agent != NULL);   /* LCOV_EXCL_BR_LINE */

    // Update state with mutex protection for thread safety
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    assert(atomic_load(&agent->state) == IK_AGENT_STATE_IDLE);   /* LCOV_EXCL_BR_LINE */
    atomic_store(&agent->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    pthread_mutex_unlock_(&agent->tool_thread_mutex);
    DEBUG_LOG("[state] uuid=%s idle->waiting_for_llm", agent->uuid);

    // Show spinner, hide input
    agent->spinner_state.visible = true;
    agent->input_buffer_visible = false;

    // Initialize spinner timestamp for time-based advancement
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    agent->spinner_state.last_advance_ms = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void ik_agent_transition_to_idle(ik_agent_ctx_t *agent)
{
    assert(agent != NULL);   /* LCOV_EXCL_BR_LINE */

    // Update state with mutex protection for thread safety
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    assert(atomic_load(&agent->state) == IK_AGENT_STATE_WAITING_FOR_LLM);   /* LCOV_EXCL_BR_LINE */
    atomic_store(&agent->state, IK_AGENT_STATE_IDLE);
    pthread_mutex_unlock_(&agent->tool_thread_mutex);
    DEBUG_LOG("[state] uuid=%s waiting_for_llm->idle", agent->uuid);

    // Hide spinner, show input
    agent->spinner_state.visible = false;
    agent->input_buffer_visible = true;
}

void ik_agent_transition_to_executing_tool(ik_agent_ctx_t *agent)
{
    assert(agent != NULL); /* LCOV_EXCL_BR_LINE */
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    assert(atomic_load(&agent->state) == IK_AGENT_STATE_WAITING_FOR_LLM); /* LCOV_EXCL_BR_LINE */
    atomic_store(&agent->state, IK_AGENT_STATE_EXECUTING_TOOL);
    pthread_mutex_unlock_(&agent->tool_thread_mutex);
    DEBUG_LOG("[state] uuid=%s waiting_for_llm->executing_tool", agent->uuid);
}

/* Append a full-width context HR (light blue) to the scrollback. */
void ik_agent_append_context_hr(ik_agent_ctx_t *agent)
{
    int cols = (agent->shared != NULL && agent->shared->term != NULL)
        ? agent->shared->term->screen_cols : 80;
    const char *label = " context ";
    int label_len = (int)strlen(label);
    int remaining = cols - label_len;
    if (remaining < 0) remaining = 0;
    int left = remaining / 2;
    int right = remaining - left;
    size_t hr_bytes = (size_t)(left + right) * 3 + (size_t)label_len + 1;
    char *hr = talloc_size(agent, hr_bytes);
    size_t pos = 0;
    for (int j = 0; j < left; j++) {
        hr[pos++] = '\xe2'; hr[pos++] = '\x94'; hr[pos++] = '\x80';
    }
    memcpy(hr + pos, label, (size_t)label_len);
    pos += (size_t)label_len;
    for (int j = 0; j < right; j++) {
        hr[pos++] = '\xe2'; hr[pos++] = '\x94'; hr[pos++] = '\x80';
    }
    hr[pos] = '\0';

    const char *hr_line = hr;
    size_t hr_line_len = pos;
    char *colored_hr = NULL;
    if (ik_ansi_colors_enabled()) {
        char color_seq[16];
        ik_ansi_fg_256(color_seq, sizeof(color_seq), 153);
        colored_hr = talloc_asprintf(agent, "%s%s%s", color_seq, hr, IK_ANSI_RESET);
        if (colored_hr == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        hr_line = colored_hr;
        hr_line_len = strlen(colored_hr);
    }

    ik_scrollback_append_line(agent->scrollback, "", 0);
    ik_scrollback_append_line(agent->scrollback, hr_line, hr_line_len);
    ik_scrollback_append_line(agent->scrollback, "", 0);
    talloc_free(colored_hr);
    talloc_free(hr);
}

/* Re-render scrollback with HR before the first active context message.
 * Called after pruning when context_start_index > 0. Follows the same
 * simplified re-render pattern as interrupt recovery. */
static void refresh_scrollback_with_hr(ik_agent_ctx_t *agent)
{
    if (agent->token_cache == NULL) PANIC("token_cache is NULL"); // LCOV_EXCL_BR_LINE
    if (agent->scrollback == NULL) return;
    size_t ctx_idx = ik_token_cache_get_context_start_index(agent->token_cache);
    if (ctx_idx == 0) return;

    ik_scrollback_clear(agent->scrollback);

    for (size_t i = 0; i < agent->message_count; i++) {
        if (i == ctx_idx) {
            /* Build a full-width HR with " context " centered.
             * ─ (U+2500) is 3 UTF-8 bytes: 0xE2 0x94 0x80. */
            ik_agent_append_context_hr(agent);
        }

        ik_message_t *m = agent->messages[i];
        if (m == NULL) PANIC("NULL message"); // LCOV_EXCL_BR_LINE
        if (m->content_count == 0) continue;

        ik_content_block_t *block = &m->content_blocks[0];
        const char *kind = m->kind;
        const char *content = NULL;

        if (block->type == IK_CONTENT_TEXT) {
            content = block->data.text.text;
        } else if (block->type == IK_CONTENT_TOOL_RESULT) {
            content = block->data.tool_result.content;
        } else if (block->type == IK_CONTENT_TOOL_CALL) {
            ik_tool_call_t tc = {
                .id        = block->data.tool_call.id,
                .name      = block->data.tool_call.name,
                .arguments = block->data.tool_call.arguments,
            };
            const char *formatted = ik_format_tool_call(agent, &tc);
            if (formatted == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            ik_event_render(agent->scrollback, "tool_call", formatted, "{}", m->interrupted);
            continue;
        }

        if (kind != NULL && content != NULL) {
            ik_event_render(agent->scrollback, kind, content, "{}", m->interrupted);
        }
    }
    ik_repl_render_usage_event(agent);
}

/* Build a malloc'd snapshot of ik_msg_t stubs from agent->messages[0..count-1].
 * Stores results in agent->summary_msgs_stubs and agent->summary_msgs_ptrs.
 * Returns the number of stubs actually populated (conversation kinds only).
 * Previous snapshot buffers are freed before building a new one. */
static size_t build_summary_msg_snapshot(ik_agent_ctx_t *agent, size_t count)
{
    /* Free any previous snapshot (from an already-completed thread). */
    free(agent->summary_msgs_stubs);
    agent->summary_msgs_stubs = NULL;
    free(agent->summary_msgs_ptrs);
    agent->summary_msgs_ptrs = NULL;

    assert(count > 0); // LCOV_EXCL_BR_LINE

    ik_msg_t *stubs = malloc(count * sizeof(ik_msg_t));
    if (stubs == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    ik_msg_t **ptrs = malloc(count * sizeof(ik_msg_t *));
    if (ptrs == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    size_t stub_count = 0;
    for (size_t i = 0; i < count; i++) {
        ik_message_t *m = agent->messages[i];
        if (m == NULL) PANIC("NULL message"); // LCOV_EXCL_BR_LINE
        if (m->content_count == 0) continue;

        ik_content_block_t *block = &m->content_blocks[0];
        const char *kind = m->kind;
        const char *content = NULL;

        if (block->type == IK_CONTENT_TEXT) {
            content = block->data.text.text;
        } else if (block->type == IK_CONTENT_TOOL_RESULT) {
            content = block->data.tool_result.content;
        }

        if (kind == NULL) PANIC("NULL message kind"); // LCOV_EXCL_BR_LINE
        if (content == NULL) continue;

        stubs[stub_count].id = 0;
        /* ik_msg_t.kind and .content are char*, not const char*. Safe to cast
         * since the worker only reads these fields (never modifies them). */
        stubs[stub_count].kind = (char *)(uintptr_t)kind;
        stubs[stub_count].content = (char *)(uintptr_t)content;
        stubs[stub_count].data_json = NULL;
        stubs[stub_count].interrupted = false;
        ptrs[stub_count] = &stubs[stub_count];
        stub_count++;
    }

    agent->summary_msgs_stubs = stubs;
    agent->summary_msgs_ptrs = ptrs;
    return stub_count;
}

void ik_agent_prune_token_cache(ik_agent_ctx_t *agent)
{
    if (agent->token_cache == NULL) return;
    int32_t budget = ik_token_cache_get_budget(agent->token_cache);
    while (ik_token_cache_get_total(agent->token_cache) > budget &&
           ik_token_cache_get_turn_count(agent->token_cache) > 1) {
        ik_token_cache_prune_oldest_turn(agent->token_cache);
    }
    refresh_scrollback_with_hr(agent);

    /* Trigger background summary regeneration if anything was pruned.
     * dispatch() creates its own independent provider instance — no shared
     * access to agent->provider_instance from the worker thread. */
    size_t ctx_idx = ik_token_cache_get_context_start_index(agent->token_cache);
    if (ctx_idx > 0 && agent->model != NULL && agent->provider != NULL) {
        size_t stub_count = build_summary_msg_snapshot(agent, ctx_idx);
        ik_summary_worker_dispatch(agent, agent->model,
                                   (ik_msg_t * const *)agent->summary_msgs_ptrs,
                                   stub_count,
                                   IK_SUMMARY_RECENT_MAX_TOKENS);
        /* If dispatch did not start a thread (e.g. provider creation failed),
         * free the snapshot buffers now — ik_summary_worker_poll will never
         * run to clean them up. */
        pthread_mutex_lock_(&agent->summary_thread_mutex);
        bool running = agent->summary_thread_running;
        pthread_mutex_unlock_(&agent->summary_thread_mutex);
        if (!running) {
            free(agent->summary_msgs_stubs);
            agent->summary_msgs_stubs = NULL;
            free(agent->summary_msgs_ptrs);
            agent->summary_msgs_ptrs = NULL;
        }
    }
}

void ik_agent_record_and_prune_token_cache(ik_agent_ctx_t *agent, bool was_success)
{
    if (!was_success || agent->token_cache == NULL) return;
    size_t turn_count = ik_token_cache_get_turn_count(agent->token_cache);
    if (turn_count > 0) {
        size_t last_turn = turn_count - 1;
        int32_t delta = agent->response_input_tokens - agent->prev_response_input_tokens;
        if (delta > 0) {
            ik_token_cache_record_turn(agent->token_cache, last_turn, delta);
        } else {
            /* Provider didn't return token counts; fall back to bytes estimate */
            int32_t est = ik_token_cache_get_turn_tokens(agent->token_cache, last_turn);
            if (est > 0) {
                ik_token_cache_record_turn(agent->token_cache, last_turn, est);
            }
        }
    }
    agent->prev_response_input_tokens = agent->response_input_tokens;
    ik_agent_prune_token_cache(agent);
}

void ik_agent_transition_from_executing_tool(ik_agent_ctx_t *agent)
{
    assert(agent != NULL); /* LCOV_EXCL_BR_LINE */
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    assert(atomic_load(&agent->state) == IK_AGENT_STATE_EXECUTING_TOOL); /* LCOV_EXCL_BR_LINE */
    atomic_store(&agent->state, IK_AGENT_STATE_WAITING_FOR_LLM);
    pthread_mutex_unlock_(&agent->tool_thread_mutex);
    DEBUG_LOG("[state] uuid=%s executing_tool->waiting_for_llm", agent->uuid);
}
