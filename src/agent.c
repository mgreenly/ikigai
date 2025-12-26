#include "agent.h"

#include "config.h"
#include "db/agent.h"
#include "db/agent_row.h"
#include "input_buffer/core.h"
#include "layer.h"
#include "layer_wrappers.h"
#include "panic.h"
#include "providers/provider.h"
#include "scrollback.h"
#include "shared.h"
#include "uuid.h"
#include "wrapper.h"
#include "wrapper_pthread.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Forward declarations to avoid type conflicts during migration
typedef struct ik_provider ik_provider_t;
extern res_t ik_provider_create(TALLOC_CTX *ctx, const char *name, ik_provider_t **out);

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

    // Initialize provider configuration (set by ik_agent_apply_defaults)
    agent->provider = NULL;
    agent->model = NULL;
    agent->thinking_level = 0;  // IK_THINKING_NONE
    agent->provider_instance = NULL;

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
    agent->messages = NULL;
    agent->message_count = 0;
    agent->message_capacity = 0;
    agent->marks = NULL;
    agent->mark_count = 0;

    // Initialize LLM interaction state (per-agent)
    agent->curl_still_running = 0;
    agent->state = IK_AGENT_STATE_IDLE;
    agent->assistant_response = NULL;
    agent->streaming_line_buffer = NULL;
    agent->http_error_message = NULL;
    agent->response_model = NULL;
    agent->response_finish_reason = NULL;
    agent->response_input_tokens = 0;
    agent->response_output_tokens = 0;
    agent->response_thinking_tokens = 0;

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

    // Initialize provider configuration (populated by ik_agent_restore_from_row)
    agent->provider = NULL;
    agent->model = NULL;
    agent->thinking_level = 0;  // IK_THINKING_NONE
    agent->provider_instance = NULL;

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
    agent->messages = NULL;
    agent->message_count = 0;
    agent->message_capacity = 0;
    agent->marks = NULL;
    agent->mark_count = 0;

    // Initialize LLM interaction state (per-agent)
    agent->curl_still_running = 0;
    agent->state = IK_AGENT_STATE_IDLE;
    agent->assistant_response = NULL;
    agent->streaming_line_buffer = NULL;
    agent->http_error_message = NULL;
    agent->response_model = NULL;
    agent->response_finish_reason = NULL;
    agent->response_input_tokens = 0;
    agent->response_output_tokens = 0;
    agent->response_thinking_tokens = 0;

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
    // Delegate to ik_agent_clone_messages which handles deep copying
    return ik_agent_clone_messages(child, parent);
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

// Helper function to parse thinking level string to enum
static int parse_thinking_level(const char *level_str)
{
    if (level_str == NULL || strcmp(level_str, "none") == 0) {
        return IK_THINKING_NONE;
    } else if (strcmp(level_str, "low") == 0) {
        return IK_THINKING_LOW;
    } else if (strcmp(level_str, "med") == 0 || strcmp(level_str, "medium") == 0) {
        return IK_THINKING_MED;
    } else if (strcmp(level_str, "high") == 0) {
        return IK_THINKING_HIGH;
    }
    // Default to none for unknown values
    return IK_THINKING_NONE;
}

res_t ik_agent_apply_defaults(ik_agent_ctx_t *agent, void *config_ptr)
{
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE

    if (config_ptr == NULL) {
        return ERR(agent, INVALID_ARG, "Config is NULL");
    }

    ik_config_t *config = (ik_config_t *)config_ptr;

    // Get default provider from config
    const char *provider = ik_config_get_default_provider(config);

    // Set provider (allocated on agent context)
    agent->provider = talloc_strdup(agent, provider);
    if (agent->provider == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // For now, use openai_model as the default model
    // TODO: This will be replaced with provider-specific defaults in future tasks
    agent->model = talloc_strdup(agent, config->openai_model);
    if (agent->model == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Default thinking level to medium
    agent->thinking_level = IK_THINKING_MED;

    // Provider instance is lazy-loaded, leave it NULL
    agent->provider_instance = NULL;

    return OK(NULL);
}

res_t ik_agent_restore_from_row(ik_agent_ctx_t *agent, const void *row_ptr)
{
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE

    if (row_ptr == NULL) {
        return ERR(agent, INVALID_ARG, "Row is NULL");
    }

    const ik_db_agent_row_t *row = (const ik_db_agent_row_t *)row_ptr;

    // Load provider, model, thinking_level from database row
    // If DB fields are NULL (old agents pre-migration), leave as NULL
    // and they will be set by caller via ik_agent_apply_defaults

    if (row->provider != NULL) {
        agent->provider = talloc_strdup(agent, row->provider);
        if (agent->provider == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    }

    if (row->model != NULL) {
        agent->model = talloc_strdup(agent, row->model);
        if (agent->model == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    }

    if (row->thinking_level != NULL) {
        agent->thinking_level = parse_thinking_level(row->thinking_level);
    }

    // Provider instance is lazy-loaded, leave it NULL
    agent->provider_instance = NULL;

    return OK(NULL);
}

res_t ik_agent_get_provider(ik_agent_ctx_t *agent, struct ik_provider **out)
{
    assert(agent != NULL);   // LCOV_EXCL_BR_LINE
    assert(out != NULL);     // LCOV_EXCL_BR_LINE

    // If already cached, return existing instance
    if (agent->provider_instance != NULL) {
        *out = agent->provider_instance;
        return OK(agent->provider_instance);
    }

    // Check if provider is configured
    if (agent->provider == NULL || strlen(agent->provider) == 0) {
        return ERR(agent, INVALID_ARG, "No provider configured");
    }

    // Create new provider instance
    ik_provider_t *provider = NULL;
    res_t res = ik_provider_create(agent, agent->provider, &provider);
    if (is_err(&res)) {
        // Provider creation failed (likely missing credentials)
        return ERR(agent, MISSING_CREDENTIALS,
                   "Failed to create provider '%s': %s",
                   agent->provider ? agent->provider : "NULL",
                   res.err->msg);
    }

    // Cache the provider instance
    agent->provider_instance = provider;
    *out = provider;

    return OK(provider);
}

void ik_agent_invalidate_provider(ik_agent_ctx_t *agent)
{
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE

    // Free cached provider instance if it exists
    if (agent->provider_instance != NULL) {
        talloc_free(agent->provider_instance);
        agent->provider_instance = NULL;
    }

    // Safe to call multiple times - idempotent
}

res_t ik_agent_add_message(ik_agent_ctx_t *agent, ik_message_t *msg)
{
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE
    assert(msg != NULL);    // LCOV_EXCL_BR_LINE

    // Grow array if needed
    if (agent->message_count >= agent->message_capacity) {
        size_t new_capacity = agent->message_capacity == 0 ? 16 : agent->message_capacity * 2;
        agent->messages = talloc_realloc(agent, agent->messages, ik_message_t *, (unsigned int)new_capacity);
        if (!agent->messages) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        agent->message_capacity = new_capacity;
    }

    // Reparent message to agent and add to array
    talloc_steal(agent, msg);
    agent->messages[agent->message_count++] = msg;

    return OK(msg);
}

void ik_agent_clear_messages(ik_agent_ctx_t *agent)
{
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE

    // Free messages array (talloc frees all children)
    if (agent->messages != NULL) {
        talloc_free(agent->messages);
        agent->messages = NULL;
    }

    agent->message_count = 0;
    agent->message_capacity = 0;
}

/**
 * Helper to clone a single content block
 */
static void clone_content_block(ik_content_block_t *dest_block,
                                const ik_content_block_t *src_block,
                                TALLOC_CTX *ctx)
{
    dest_block->type = src_block->type;

    if (src_block->type == IK_CONTENT_TEXT) {
        dest_block->data.text.text = talloc_strdup(ctx, src_block->data.text.text);
        if (!dest_block->data.text.text) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    } else if (src_block->type == IK_CONTENT_TOOL_CALL) {
        dest_block->data.tool_call.id = talloc_strdup(ctx, src_block->data.tool_call.id);
        if (!dest_block->data.tool_call.id) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        dest_block->data.tool_call.name = talloc_strdup(ctx, src_block->data.tool_call.name);
        if (!dest_block->data.tool_call.name) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        dest_block->data.tool_call.arguments = talloc_strdup(ctx, src_block->data.tool_call.arguments);
        if (!dest_block->data.tool_call.arguments) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    } else if (src_block->type == IK_CONTENT_TOOL_RESULT) {
        dest_block->data.tool_result.tool_call_id = talloc_strdup(ctx, src_block->data.tool_result.tool_call_id);
        if (!dest_block->data.tool_result.tool_call_id) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        dest_block->data.tool_result.content = talloc_strdup(ctx, src_block->data.tool_result.content);
        if (!dest_block->data.tool_result.content) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        dest_block->data.tool_result.is_error = src_block->data.tool_result.is_error;
    } else if (src_block->type == IK_CONTENT_THINKING) {
        dest_block->data.thinking.text = talloc_strdup(ctx, src_block->data.thinking.text);
        if (!dest_block->data.thinking.text) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    }
}

/**
 * Helper to clone a single message
 */
static ik_message_t *clone_message(const ik_message_t *src_msg, TALLOC_CTX *ctx)
{
    ik_message_t *dest_msg = talloc_zero(ctx, ik_message_t);
    if (!dest_msg) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    dest_msg->role = src_msg->role;
    dest_msg->content_count = src_msg->content_count;
    dest_msg->content_blocks = talloc_array(dest_msg, ik_content_block_t,
                                            (unsigned int)src_msg->content_count);
    if (!dest_msg->content_blocks) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    for (size_t j = 0; j < src_msg->content_count; j++) {
        clone_content_block(&dest_msg->content_blocks[j], &src_msg->content_blocks[j], dest_msg);
    }

    if (src_msg->provider_metadata != NULL) {
        dest_msg->provider_metadata = talloc_strdup(dest_msg, src_msg->provider_metadata);
        if (!dest_msg->provider_metadata) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    } else {
        dest_msg->provider_metadata = NULL;
    }

    return dest_msg;
}

res_t ik_agent_clone_messages(ik_agent_ctx_t *dest, const ik_agent_ctx_t *src)
{
    assert(dest != NULL);  // LCOV_EXCL_BR_LINE
    assert(src != NULL);   // LCOV_EXCL_BR_LINE

    ik_agent_clear_messages(dest);

    if (src->message_count == 0) {
        return OK(NULL);
    }

    dest->messages = talloc_array(dest, ik_message_t *, (unsigned int)src->message_count);
    if (!dest->messages) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    dest->message_capacity = src->message_count;

    for (size_t i = 0; i < src->message_count; i++) {
        dest->messages[i] = clone_message(src->messages[i], dest);
    }

    dest->message_count = src->message_count;
    return OK(dest->messages);
}
