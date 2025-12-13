#include "agent.h"

#include "input_buffer/core.h"
#include "layer.h"
#include "layer_wrappers.h"
#include "openai/client.h"
#include "openai/client_multi.h"
#include "panic.h"
#include "scrollback.h"
#include "shared.h"
#include "wrapper.h"
#include "wrapper_pthread.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Base64url alphabet (RFC 4648 section 5)
static const char BASE64URL[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

char *ik_agent_generate_uuid(TALLOC_CTX *ctx)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE

    // Generate 16 random bytes (128-bit UUID v4)
    unsigned char bytes[16];
    for (int i = 0; i < 16; i++) {
        bytes[i] = (unsigned char)(rand() & 0xFF);
    }

    // Set version (4) and variant (RFC 4122)
    bytes[6] = (bytes[6] & 0x0F) | 0x40;  // Version 4
    bytes[8] = (bytes[8] & 0x3F) | 0x80;  // Variant 1

    // Encode 16 bytes to 22 base64url characters (no padding)
    // 16 bytes = 128 bits, base64 encodes 6 bits per char
    // ceil(128/6) = 22 characters
    char *uuid = talloc_array(ctx, char, 23);  // 22 chars + null
    if (uuid == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    int j = 0;
    for (int i = 0; i < 16; i += 3) {
        uint32_t n = ((uint32_t)bytes[i] << 16);
        if (i + 1 < 16) n |= ((uint32_t)bytes[i + 1] << 8);
        if (i + 2 < 16) n |= bytes[i + 2];

        uuid[j++] = BASE64URL[(n >> 18) & 0x3F];
        uuid[j++] = BASE64URL[(n >> 12) & 0x3F];
        if (i + 1 < 16) uuid[j++] = BASE64URL[(n >> 6) & 0x3F];
        if (i + 2 < 16) uuid[j++] = BASE64URL[n & 0x3F];
    }
    uuid[j] = '\0';

    return uuid;
}

static int agent_destructor(ik_agent_ctx_t *agent)
{
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

    agent->uuid = ik_agent_generate_uuid(agent);
    agent->name = NULL;  // Unnamed by default
    agent->parent_uuid = parent_uuid ? talloc_strdup(agent, parent_uuid) : NULL;
    agent->shared = shared;

    // Initialize display state
    // Use default terminal width (80) if shared->term is not yet initialized
    int32_t term_cols = (shared->term != NULL) ? shared->term->screen_cols : 80;
    int32_t term_rows = (shared->term != NULL) ? shared->term->screen_rows : 24;

    agent->scrollback = ik_scrollback_create(agent, term_cols);
    agent->layer_cake = ik_layer_cake_create(agent, (size_t)term_rows);

    // Initialize input state (per-agent - preserves partial composition)
    agent->input_buffer = ik_input_buffer_create(agent);
    agent->separator_visible = true;
    agent->input_buffer_visible = true;
    agent->input_text = NULL;
    agent->input_text_len = 0;

    // Initialize conversation state (per-agent)
    agent->conversation = ik_openai_conversation_create(agent).ok;
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

    // Create and add layers (following pattern from repl_init.c)
    agent->scrollback_layer = ik_scrollback_layer_create(agent, "scrollback", agent->scrollback);
    res_t result = ik_layer_cake_add_layer(agent->layer_cake, agent->scrollback_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    // Create spinner layer (initially hidden)
    ik_spinner_state_t *spinner_state = talloc_zero_(agent, sizeof(ik_spinner_state_t));
    if (spinner_state == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    spinner_state->frame_index = 0;
    spinner_state->visible = false;
    agent->spinner_layer = ik_spinner_layer_create(agent, "spinner", spinner_state);
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

    // Create completion layer
    ik_completion_t **completion_ptr = talloc_zero_(agent, sizeof(ik_completion_t *));
    if (completion_ptr == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    *completion_ptr = NULL;
    agent->completion_layer = ik_completion_layer_create(agent, "completion", completion_ptr);
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
    if (mutex_result != 0) {
        talloc_free(agent);
        return ERR(ctx, IO, "Failed to initialize tool thread mutex");
    }

    // Set destructor to clean up mutex
    talloc_set_destructor(agent, agent_destructor);

    *out = agent;
    return OK(agent);
}
