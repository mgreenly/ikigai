#include "agent.h"
#include "config.h"
#include "config_defaults.h"
#include "db/agent.h"
#include "db/agent_row.h"
#include "doc_cache.h"
#include "file_utils.h"
#include "input_buffer/core.h"
#include "layer.h"
#include "layer_wrappers.h"
#include "panic.h"
#include "paths.h"
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
    agent->banner_visible = true;
    agent->separator_visible = true;
    agent->input_buffer_visible = true;
    agent->status_visible = true;
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
    // Banner layer must be first (topmost)
    agent->banner_layer = ik_banner_layer_create(agent, "banner", &agent->banner_visible);
    res_t result = ik_layer_cake_add_layer(agent->layer_cake, agent->banner_layer);
    if (is_err(&result)) PANIC("OOM"); /* LCOV_EXCL_BR_LINE */

    agent->scrollback_layer = ik_scrollback_layer_create(agent, "scrollback", agent->scrollback);
    result = ik_layer_cake_add_layer(agent->layer_cake, agent->scrollback_layer);
    if (is_err(&result)) PANIC("OOM"); /* LCOV_EXCL_BR_LINE */

    // Create spinner layer (pass pointer to agent's spinner_state)
    agent->spinner_layer = ik_spinner_layer_create(agent, "spinner", &agent->spinner_state);
    result = ik_layer_cake_add_layer(agent->layer_cake, agent->spinner_layer);
    if (is_err(&result)) PANIC("OOM"); /* LCOV_EXCL_BR_LINE */

    // Create separator layer (upper) - pass pointer to agent field
    agent->separator_layer = ik_separator_layer_create(agent, "separator", &agent->separator_visible);
    result = ik_layer_cake_add_layer(agent->layer_cake, agent->separator_layer);
    if (is_err(&result)) PANIC("OOM"); /* LCOV_EXCL_BR_LINE */

    // Create input layer - pass pointers to agent fields
    agent->input_layer = ik_input_layer_create(agent, "input",
                                               &agent->input_buffer_visible,
                                               &agent->input_text,
                                               &agent->input_text_len);
    result = ik_layer_cake_add_layer(agent->layer_cake, agent->input_layer);
    if (is_err(&result)) PANIC("OOM"); /* LCOV_EXCL_BR_LINE */

    // Create completion layer (pass pointer to agent's completion field)
    agent->completion_layer = ik_completion_layer_create(agent, "completion", &agent->completion);
    result = ik_layer_cake_add_layer(agent->layer_cake, agent->completion_layer);
    if (is_err(&result)) PANIC("OOM"); /* LCOV_EXCL_BR_LINE */

    agent->status_layer = ik_status_layer_create(agent, "status", &agent->status_visible, &agent->model, &agent->thinking_level);
    result = ik_layer_cake_add_layer(agent->layer_cake, agent->status_layer);
    if (is_err(&result)) PANIC("OOM"); /* LCOV_EXCL_BR_LINE */

    // Initialize viewport offset
    agent->viewport_offset = 0;

    // Initialize pending thinking fields
    agent->pending_thinking_text = NULL;
    agent->pending_thinking_signature = NULL;
    agent->pending_redacted_data = NULL;

    // Initialize tool execution state
    agent->pending_tool_call = NULL;
    agent->pending_tool_thought_signature = NULL;
    agent->tool_thread_running = false;
    agent->tool_thread_complete = false;
    agent->tool_thread_ctx = NULL;
    agent->tool_thread_result = NULL;
    agent->tool_iteration_count = 0;
    agent->tool_child_pid = 0;
    agent->interrupt_requested = false;

    // Initialize pinned documents state
    agent->pinned_paths = NULL;
    agent->pinned_count = 0;
    agent->doc_cache = (shared->paths != NULL) ? ik_doc_cache_create(agent, shared->paths) : NULL;

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
    agent->banner_visible = true;
    agent->separator_visible = true;
    agent->input_buffer_visible = true;
    agent->status_visible = true;
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
    // Banner layer must be first (topmost)
    agent->banner_layer = ik_banner_layer_create(agent, "banner", &agent->banner_visible);
    res_t result = ik_layer_cake_add_layer(agent->layer_cake, agent->banner_layer);
    if (is_err(&result)) PANIC("OOM"); /* LCOV_EXCL_BR_LINE */

    agent->scrollback_layer = ik_scrollback_layer_create(agent, "scrollback", agent->scrollback);
    result = ik_layer_cake_add_layer(agent->layer_cake, agent->scrollback_layer);
    if (is_err(&result)) PANIC("OOM"); /* LCOV_EXCL_BR_LINE */

    // Create spinner layer (pass pointer to agent's spinner_state)
    agent->spinner_layer = ik_spinner_layer_create(agent, "spinner", &agent->spinner_state);
    result = ik_layer_cake_add_layer(agent->layer_cake, agent->spinner_layer);
    if (is_err(&result)) PANIC("OOM"); /* LCOV_EXCL_BR_LINE */

    // Create separator layer (upper) - pass pointer to agent field
    agent->separator_layer = ik_separator_layer_create(agent, "separator", &agent->separator_visible);
    result = ik_layer_cake_add_layer(agent->layer_cake, agent->separator_layer);
    if (is_err(&result)) PANIC("OOM"); /* LCOV_EXCL_BR_LINE */

    // Create input layer - pass pointers to agent fields
    agent->input_layer = ik_input_layer_create(agent, "input",
                                               &agent->input_buffer_visible,
                                               &agent->input_text,
                                               &agent->input_text_len);
    result = ik_layer_cake_add_layer(agent->layer_cake, agent->input_layer);
    if (is_err(&result)) PANIC("OOM"); /* LCOV_EXCL_BR_LINE */

    // Create completion layer (pass pointer to agent's completion field)
    agent->completion_layer = ik_completion_layer_create(agent, "completion", &agent->completion);
    result = ik_layer_cake_add_layer(agent->layer_cake, agent->completion_layer);
    if (is_err(&result)) PANIC("OOM"); /* LCOV_EXCL_BR_LINE */

    agent->status_layer = ik_status_layer_create(agent, "status", &agent->status_visible, &agent->model, &agent->thinking_level);
    result = ik_layer_cake_add_layer(agent->layer_cake, agent->status_layer);
    if (is_err(&result)) PANIC("OOM"); /* LCOV_EXCL_BR_LINE */

    // Initialize viewport offset
    agent->viewport_offset = 0;

    // Initialize tool execution state
    agent->pending_tool_call = NULL;
    agent->tool_thread_running = false;
    agent->tool_thread_complete = false;
    agent->tool_thread_ctx = NULL;
    agent->tool_thread_result = NULL;
    agent->tool_iteration_count = 0;
    agent->tool_child_pid = 0;
    agent->interrupt_requested = false;

    // Initialize pinned documents state
    agent->pinned_paths = NULL;
    agent->pinned_count = 0;
    agent->doc_cache = (shared->paths != NULL) ? ik_doc_cache_create(agent, shared->paths) : NULL;

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

// State transition functions moved to agent_state.c
// Provider and configuration functions moved to agent_provider.c
// Message management functions moved to agent_messages.c

res_t ik_agent_get_effective_system_prompt(ik_agent_ctx_t *agent, char **out)
{
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE
    assert(out != NULL);    // LCOV_EXCL_BR_LINE

    *out = NULL;

    // Priority 1: Pinned files (if any)
    if (agent->pinned_count > 0 && agent->doc_cache != NULL) {
        char *assembled = talloc_strdup(agent, "");
        if (assembled == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        for (size_t i = 0; i < agent->pinned_count; i++) {
            const char *path = agent->pinned_paths[i];
            char *content = NULL;
            res_t doc_res = ik_doc_cache_get(agent->doc_cache, path, &content);

            if (is_ok(&doc_res) && content != NULL) {
                char *new_assembled = talloc_asprintf(agent, "%s%s", assembled, content);
                if (new_assembled == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
                talloc_free(assembled);
                assembled = new_assembled;
            }
        }

        if (strlen(assembled) > 0) {
            *out = assembled;
            return OK(*out);
        }
        talloc_free(assembled);
    }

    // Priority 2: $IKIGAI_DATA_DIR/system/prompt.md
    if (agent->shared != NULL && agent->shared->paths != NULL) {
        const char *data_dir = ik_paths_get_data_dir(agent->shared->paths);
        char *prompt_path = talloc_asprintf(agent, "%s/system/prompt.md", data_dir);
        if (prompt_path == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        char *content = NULL;
        res_t read_res = ik_file_read_all(agent, prompt_path, &content, NULL);
        talloc_free(prompt_path);

        if (is_ok(&read_res) && content != NULL && strlen(content) > 0) {
            *out = content;
            return OK(*out);
        }
        if (content != NULL) {
            talloc_free(content);
        }
    }

    // Priority 3: Config fallback
    if (agent->shared != NULL && agent->shared->cfg != NULL &&
        agent->shared->cfg->openai_system_message != NULL) {
        *out = talloc_strdup(agent, agent->shared->cfg->openai_system_message);
        if (*out == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return OK(*out);
    }

    // Priority 4: Hardcoded default
    *out = talloc_strdup(agent, IK_DEFAULT_OPENAI_SYSTEM_MESSAGE);
    if (*out == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    return OK(*out);
}
