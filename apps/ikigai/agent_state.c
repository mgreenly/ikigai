#include "apps/ikigai/agent.h"

#include "apps/ikigai/event_render.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/token_cache.h"
#include "apps/ikigai/wrapper_pthread.h"

#include "apps/ikigai/debug_log.h"

#include <assert.h>
#include <inttypes.h>
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

/* Horizontal rule text inserted before the first active context message */
#define IK_CONTEXT_HR "── context ──"

/* Re-render scrollback with HR before the first active context message.
 * Called after pruning when context_start_index > 0. Follows the same
 * simplified re-render pattern as interrupt recovery. */
static void refresh_scrollback_with_hr(ik_agent_ctx_t *agent)
{
    if (agent->token_cache == NULL || agent->scrollback == NULL) return;
    size_t ctx_idx = ik_token_cache_get_context_start_index(agent->token_cache);
    if (ctx_idx == 0) return;

    ik_scrollback_clear(agent->scrollback);

    for (size_t i = 0; i < agent->message_count; i++) {
        if (i == ctx_idx) {
            ik_scrollback_append_line(agent->scrollback, "", 0);
            ik_scrollback_append_line(agent->scrollback,
                                      IK_CONTEXT_HR, strlen(IK_CONTEXT_HR));
            ik_scrollback_append_line(agent->scrollback, "", 0);
        }

        ik_message_t *m = agent->messages[i];
        if (m == NULL || m->content_count == 0) continue;

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
