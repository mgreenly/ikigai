#include "apps/ikigai/agent.h"

#include "apps/ikigai/token_cache.h"
#include "apps/ikigai/wrapper_pthread.h"

#include "apps/ikigai/debug_log.h"

#include <assert.h>
#include <inttypes.h>
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

void ik_agent_prune_token_cache(ik_agent_ctx_t *agent)
{
    if (agent->token_cache == NULL) return;
    int32_t budget = ik_token_cache_get_budget(agent->token_cache);
    while (ik_token_cache_get_total(agent->token_cache) > budget &&
           ik_token_cache_get_turn_count(agent->token_cache) > 1) {
        ik_token_cache_prune_oldest_turn(agent->token_cache);
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
