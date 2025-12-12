#include "agent.h"

#include "layer.h"
#include "layer_wrappers.h"
#include "panic.h"
#include "scrollback.h"
#include "shared.h"
#include "wrapper.h"

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

    // Create separator layer (upper)
    bool *separator_visible = talloc_zero_(agent, sizeof(bool));
    if (separator_visible == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    *separator_visible = true;
    agent->separator_layer = ik_separator_layer_create(agent, "separator", separator_visible);
    result = ik_layer_cake_add_layer(agent->layer_cake, agent->separator_layer);
    if (is_err(&result)) PANIC("allocation failed"); /* LCOV_EXCL_BR_LINE */

    // Create input layer
    bool *input_visible = talloc_zero_(agent, sizeof(bool));
    if (input_visible == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    *input_visible = true;
    const char **input_text = talloc_zero_(agent, sizeof(const char *));
    if (input_text == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    *input_text = "";
    size_t *input_text_len = talloc_zero_(agent, sizeof(size_t));
    if (input_text_len == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    *input_text_len = 0;
    agent->input_layer = ik_input_layer_create(agent, "input", input_visible, input_text, input_text_len);
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

    *out = agent;
    return OK(agent);
}
