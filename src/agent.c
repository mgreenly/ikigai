#include "agent.h"

#include "db/agent.h"
#include "input_buffer/core.h"
#include "layer.h"
#include "layer_wrappers.h"
#include "openai/client.h"
#include "openai/client_multi.h"
#include "panic.h"
#include "scrollback.h"
#include "shared.h"
#include "uuid.h"
#include "wrapper.h"
#include "wrapper_pthread.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static int agent_destructor(ik_agent_ctx_t *agent)
{
    // Ensure mutex is unlocked before destruction (helgrind requirement)
    // The mutex should already be unlocked because repl_destructor waits
    // for tool thread completion, but we verify it here for safety.
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    pthread_mutex_unlock_(&agent->tool_thread_mutex);
    pthread_mutex_destroy_(&agent->tool_thread_mutex);
    return 0;
}

res_t ik_agent_create(TALLOC_CTX *ctx, ik_shared_ctx_t *shared,
                      const char *parent_uuid, ik_agent_ctx_t **out)
{
    assert(ctx != NULL);    // LCOV_EXCL_BR_LINE
    assert(shared != NULL); // LCOV_EXCL_BR_LINE
    assert(out != NULL);    // LCOV_EXCL_BR_LINE

    ik_agent_ctx_t *agent = talloc_zero_(ctx, sizeof(ik_agent_ctx_t));
    if (agent == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    agent->uuid = ik_generate_uuid(agent);
    agent->name = NULL;  // Unnamed by default
    agent->parent_uuid = parent_uuid ? talloc_strdup(agent, parent_uuid) : NULL;
    agent->shared = shared;
    agent->repl = NULL;  // Set by caller after creation
    agent->created_at = time(NULL);

    // Initialize display state
    // Use default terminal width (80) if shared->term is not yet initialized
    int32_t term_cols = (shared->term != NULL) ? shared->term->screen_cols : 80;     // LCOV_EXCL_BR_LINE
    int32_t term_rows = (shared->term != NULL) ? shared->term->screen_rows : 24;     // LCOV_EXCL_BR_LINE

    agent->scrollback = ik_scrollback_create(agent, term_cols);
    agent->layer_cake = ik_layer_cake_create(agent, (size_t)term_rows);

    // Initialize input state (per-agent - preserves partial composition)
    agent->input_buffer = ik_input_buffer_create(agent);
    agent->separator_visible = true;
    agent->input_buffer_visible = true;
    agent->input_text = NULL;
    agent->input_text_len = 0;

    // Initialize tab completion state (per-agent)
    agent->completion = NULL;  // Created on Tab press, destroyed on completion

    // Initialize conversation state (per-agent)
    agent->conversation = ik_openai_conversation_create(agent);
    agent->marks = NULL;
    agent->mark_count = 0;

    // Initialize LLM interaction state (per-agent)
    agent->multi = ik_openai_multi_create(agent).ok;  // Created immediately for event loop
    if (agent->multi == NULL) PANIC("Failed to create curl_multi handle");  // LCOV_EXCL_BR_LINE
    agent->curl_still_running = 0;
    agent->state = IK_AGENT_STATE_IDLE;
    agent->assistant_response = NULL;
    agent->streaming_line_buffer = NULL;
    agent->http_error_message = NULL;
    agent->response_model = NULL;
    agent->response_finish_reason = NULL;
    agent->response_completion_tokens = 0;

    // Initialize spinner state (per-agent - embedded in agent struct)
    agent->spinner_state.frame_index = 0;
    agent->spinner_state.visible = false;

    // Create and add layers (following pattern from repl_init.c)
    agent->scrollback_layer = ik_scrollback_layer_create(agent, "scrollback", agent->scrollback);
    res_t result = ik_layer_cake_add_layer(agent->layer_cake, agent->scrollback_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    // Create spinner layer (pass pointer to agent's spinner_state)
    agent->spinner_layer = ik_spinner_layer_create(agent, "spinner", &agent->spinner_state);
    result = ik_layer_cake_add_layer(agent->layer_cake, agent->spinner_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    // Create separator layer (upper) - pass pointer to agent field
    agent->separator_layer = ik_separator_layer_create(agent, "separator", &agent->separator_visible);
    result = ik_layer_cake_add_layer(agent->layer_cake, agent->separator_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    // Create input layer - pass pointers to agent fields
    agent->input_layer = ik_input_layer_create(agent, "input",
                                               &agent->input_buffer_visible,
                                               &agent->input_text,
                                               &agent->input_text_len);
    result = ik_layer_cake_add_layer(agent->layer_cake, agent->input_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    // Create completion layer (pass pointer to agent's completion field)
    agent->completion_layer = ik_completion_layer_create(agent, "completion", &agent->completion);
    result = ik_layer_cake_add_layer(agent->layer_cake, agent->completion_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    // Initialize viewport offset
    agent->viewport_offset = 0;

    // Initialize tool execution state
    agent->pending_tool_call = NULL;
    agent->tool_thread_running = false;
    agent->tool_thread_complete = false;
    agent->tool_thread_ctx = NULL;
    agent->tool_thread_result = NULL;
    agent->tool_iteration_count = 0;

    int mutex_result = pthread_mutex_init_(&agent->tool_thread_mutex, NULL);
    if (mutex_result != 0) {     // LCOV_EXCL_BR_LINE - Pthread failure tested in pthread tests
        // Free agent without calling destructor (mutex not initialized yet)
        // We need to explicitly free here because agent is allocated on ctx
        talloc_free(agent);     // LCOV_EXCL_LINE
        *out = NULL;     // LCOV_EXCL_LINE
        return ERR(ctx, IO, "Failed to initialize tool thread mutex");     // LCOV_EXCL_LINE
    }

    // Set destructor to clean up mutex (only after successful init)
    talloc_set_destructor(agent, agent_destructor);

    *out = agent;
    return OK(agent);
}

res_t ik_agent_restore(TALLOC_CTX *ctx, ik_shared_ctx_t *shared,
                       const void *row_ptr, ik_agent_ctx_t **out)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(shared != NULL);   // LCOV_EXCL_BR_LINE
    assert(row_ptr != NULL);  // LCOV_EXCL_BR_LINE
    assert(out != NULL);      // LCOV_EXCL_BR_LINE

    const ik_db_agent_row_t *row = row_ptr;
    assert(row->uuid != NULL); // LCOV_EXCL_BR_LINE

    ik_agent_ctx_t *agent = talloc_zero_(ctx, sizeof(ik_agent_ctx_t));
    if (agent == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Restore identity from database row (instead of generating new values)
    agent->uuid = talloc_strdup(agent, row->uuid);
    agent->name = row->name ? talloc_strdup(agent, row->name) : NULL;
    agent->parent_uuid = row->parent_uuid ? talloc_strdup(agent, row->parent_uuid) : NULL;
    agent->created_at = row->created_at;
    agent->fork_message_id = row->fork_message_id ? strtoll(row->fork_message_id, NULL, 10) : 0;
    agent->shared = shared;
    agent->repl = NULL;  // Set by caller after restore

    // Initialize display state
    // Use default terminal width (80) if shared->term is not yet initialized
    int32_t term_cols = (shared->term != NULL) ? shared->term->screen_cols : 80;     // LCOV_EXCL_BR_LINE
    int32_t term_rows = (shared->term != NULL) ? shared->term->screen_rows : 24;     // LCOV_EXCL_BR_LINE

    agent->scrollback = ik_scrollback_create(agent, term_cols);
    agent->layer_cake = ik_layer_cake_create(agent, (size_t)term_rows);

    // Initialize input state (per-agent - preserves partial composition)
    agent->input_buffer = ik_input_buffer_create(agent);
    agent->separator_visible = true;
    agent->input_buffer_visible = true;
    agent->input_text = NULL;
    agent->input_text_len = 0;

    // Initialize tab completion state (per-agent)
    agent->completion = NULL;  // Created on Tab press, destroyed on completion

    // Initialize conversation state (per-agent)
    agent->conversation = ik_openai_conversation_create(agent);
    agent->marks = NULL;
    agent->mark_count = 0;

    // Initialize LLM interaction state (per-agent)
    agent->multi = ik_openai_multi_create(agent).ok;  // Created immediately for event loop
    if (agent->multi == NULL) PANIC("Failed to create curl_multi handle");  // LCOV_EXCL_BR_LINE
    agent->curl_still_running = 0;
    agent->state = IK_AGENT_STATE_IDLE;
    agent->assistant_response = NULL;
    agent->streaming_line_buffer = NULL;
    agent->http_error_message = NULL;
    agent->response_model = NULL;
    agent->response_finish_reason = NULL;
    agent->response_completion_tokens = 0;

    // Initialize spinner state (per-agent - embedded in agent struct)
    agent->spinner_state.frame_index = 0;
    agent->spinner_state.visible = false;

    // Create and add layers (following pattern from repl_init.c)
    agent->scrollback_layer = ik_scrollback_layer_create(agent, "scrollback", agent->scrollback);
    res_t result = ik_layer_cake_add_layer(agent->layer_cake, agent->scrollback_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    // Create spinner layer (pass pointer to agent's spinner_state)
    agent->spinner_layer = ik_spinner_layer_create(agent, "spinner", &agent->spinner_state);
    result = ik_layer_cake_add_layer(agent->layer_cake, agent->spinner_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    // Create separator layer (upper) - pass pointer to agent field
    agent->separator_layer = ik_separator_layer_create(agent, "separator", &agent->separator_visible);
    result = ik_layer_cake_add_layer(agent->layer_cake, agent->separator_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    // Create input layer - pass pointers to agent fields
    agent->input_layer = ik_input_layer_create(agent, "input",
                                               &agent->input_buffer_visible,
                                               &agent->input_text,
                                               &agent->input_text_len);
    result = ik_layer_cake_add_layer(agent->layer_cake, agent->input_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    // Create completion layer (pass pointer to agent's completion field)
    agent->completion_layer = ik_completion_layer_create(agent, "completion", &agent->completion);
    result = ik_layer_cake_add_layer(agent->layer_cake, agent->completion_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    // Initialize viewport offset
    agent->viewport_offset = 0;

    // Initialize tool execution state
    agent->pending_tool_call = NULL;
    agent->tool_thread_running = false;
    agent->tool_thread_complete = false;
    agent->tool_thread_ctx = NULL;
    agent->tool_thread_result = NULL;
    agent->tool_iteration_count = 0;

    int mutex_result = pthread_mutex_init_(&agent->tool_thread_mutex, NULL);
    if (mutex_result != 0) {     // LCOV_EXCL_BR_LINE - Pthread failure tested in pthread tests
        // Free agent without calling destructor (mutex not initialized yet)
        // We need to explicitly free here because agent is allocated on ctx
        talloc_free(agent);     // LCOV_EXCL_LINE
        *out = NULL;     // LCOV_EXCL_LINE
        return ERR(ctx, IO, "Failed to initialize tool thread mutex");     // LCOV_EXCL_LINE
    }

    // Set destructor to clean up mutex (only after successful init)
    talloc_set_destructor(agent, agent_destructor);

    *out = agent;
    return OK(agent);
}

res_t ik_agent_copy_conversation(ik_agent_ctx_t *child, const ik_agent_ctx_t *parent)
{
    assert(child != NULL);   // LCOV_EXCL_BR_LINE
    assert(parent != NULL);  // LCOV_EXCL_BR_LINE
    assert(child->conversation != NULL);   // LCOV_EXCL_BR_LINE
    assert(parent->conversation != NULL);  // LCOV_EXCL_BR_LINE

    // Copy each message from parent to child
    for (size_t i = 0; i < parent->conversation->message_count; i++) {
        ik_msg_t *src_msg = parent->conversation->messages[i];

        // Create a new message with copied content
        ik_msg_t *new_msg = ik_openai_msg_create(child->conversation, src_msg->kind, src_msg->content);

        // Copy data_json if present
        if (src_msg->data_json != NULL) {
            new_msg->data_json = talloc_strdup(new_msg, src_msg->data_json);
            if (new_msg->data_json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        }

        // Add to child's conversation
        res_t add_res = ik_openai_conversation_add_msg(child->conversation, new_msg);
        if (is_err(&add_res)) {     // LCOV_EXCL_BR_LINE
            return add_res;     // LCOV_EXCL_LINE
        }
    }

    return OK(NULL);
}

bool ik_agent_has_running_tools(const ik_agent_ctx_t *agent)
{
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE
    return agent->tool_thread_running;
}

// State transition functions (moved from repl.c)
// These now operate on a specific agent instead of repl->current,
// enabling proper multi-agent support.

void ik_agent_transition_to_waiting_for_llm(ik_agent_ctx_t *agent)
{
    assert(agent != NULL);   /* LCOV_EXCL_BR_LINE */

    // Update state with mutex protection for thread safety
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    assert(agent->state == IK_AGENT_STATE_IDLE);   /* LCOV_EXCL_BR_LINE */
    agent->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);

    // Show spinner, hide input
    agent->spinner_state.visible = true;
    agent->input_buffer_visible = false;
}

void ik_agent_transition_to_idle(ik_agent_ctx_t *agent)
{
    assert(agent != NULL);   /* LCOV_EXCL_BR_LINE */

    // Update state with mutex protection for thread safety
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    assert(agent->state == IK_AGENT_STATE_WAITING_FOR_LLM);   /* LCOV_EXCL_BR_LINE */
    agent->state = IK_AGENT_STATE_IDLE;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);

    // Hide spinner, show input
    agent->spinner_state.visible = false;
    agent->input_buffer_visible = true;
}

void ik_agent_transition_to_executing_tool(ik_agent_ctx_t *agent)
{
    assert(agent != NULL); /* LCOV_EXCL_BR_LINE */
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    assert(agent->state == IK_AGENT_STATE_WAITING_FOR_LLM); /* LCOV_EXCL_BR_LINE */
    agent->state = IK_AGENT_STATE_EXECUTING_TOOL;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);
}

void ik_agent_transition_from_executing_tool(ik_agent_ctx_t *agent)
{
    assert(agent != NULL); /* LCOV_EXCL_BR_LINE */
    pthread_mutex_lock_(&agent->tool_thread_mutex);
    assert(agent->state == IK_AGENT_STATE_EXECUTING_TOOL); /* LCOV_EXCL_BR_LINE */
    agent->state = IK_AGENT_STATE_WAITING_FOR_LLM;
    pthread_mutex_unlock_(&agent->tool_thread_mutex);
}
