#include "agent.h"
/**
 * @file repl_streaming_test_common.c
 * @brief Common mock infrastructure implementation for streaming tests
 */

#include "repl_streaming_test_common.h"
#include "../../../src/agent.h"
#include "../../../src/shared.h"

// Global state for curl mocking
curl_write_callback g_write_callback = NULL;
void *g_write_data = NULL;
const char *mock_response_data = NULL;
size_t mock_response_len = 0;
bool invoke_write_callback = false;
CURL *g_last_easy_handle = NULL;
bool simulate_completion = false;
bool mock_write_should_fail = false;

// Mock write wrapper
ssize_t posix_write_(int fd, const void *buf, size_t count)
{
    (void)fd;
    (void)buf;
    if (mock_write_should_fail) {
        return -1;
    }
    return (ssize_t)count;
}

/* Override curl_easy_init_ to capture handle */
CURL *curl_easy_init_(void)
{
    CURL *handle = curl_easy_init();
    g_last_easy_handle = handle;
    return handle;
}

/* Override curl_easy_setopt_ to capture write callback */
CURLcode curl_easy_setopt_(CURL *curl, CURLoption opt, const void *val)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
    if (opt == CURLOPT_WRITEFUNCTION) {
        g_write_callback = (curl_write_callback)val;
    } else if (opt == CURLOPT_WRITEDATA) {
        g_write_data = (void *)val;
    }
#pragma GCC diagnostic pop

    return curl_easy_setopt(curl, opt, val);
}

/* Override curl_multi_perform_ to invoke write callback when requested */
CURLMcode curl_multi_perform_(CURLM *multi, int *running_handles)
{
    (void)multi;  // Unused in mock

    /* Invoke write callback if requested for testing */
    if (invoke_write_callback && g_write_callback && mock_response_data) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
        g_write_callback((char *)mock_response_data, 1, mock_response_len, g_write_data);
#pragma GCC diagnostic pop
    }

    /* Simulate request completion if requested */
    if (simulate_completion) {
        *running_handles = 0;
        return CURLM_OK;
    }

    /* If not simulating completion and handles was > 0, keep it > 0 */
    if (*running_handles > 0) {
        /* Keep the current value - don't let it drop to 0 unless we're simulating completion */
        return CURLM_OK;
    }

    /* If handles is 0, just return OK (no real curl calls in tests) */
    return CURLM_OK;
}

// Helper function to create a REPL context with all LLM components
ik_repl_ctx_t *create_test_repl_with_llm(void *ctx)
{
    res_t res;

    // Create render context
    ik_render_ctx_t *render = NULL;
    res = ik_render_create(ctx, 24, 80, 1, &render);
    ck_assert(is_ok(&res));

    // Create term context
    ik_term_ctx_t *term = talloc_zero(ctx, ik_term_ctx_t);
    term->tty_fd = 1;  // stdout for tests
    term->screen_rows = 24;
    term->screen_cols = 80;

    // Create scrollback
    ik_scrollback_t *scrollback = ik_scrollback_create(ctx, 80);

    // Create layer cake and layers
    ik_layer_cake_t *layer_cake = NULL;
    layer_cake = ik_layer_cake_create(ctx, 24);

    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->term = term;
    shared->render = render;

    // Create agent (includes input_buffer)
    ik_agent_ctx_t *agent = NULL;
    res = ik_test_create_agent(ctx, &agent);
    ck_assert(is_ok(&res));

    // Override agent's display state with test fixtures
    talloc_free(agent->scrollback);
    agent->scrollback = scrollback;
    talloc_free(agent->layer_cake);
    agent->layer_cake = layer_cake;
    agent->viewport_offset = 0;

    // Create REPL context
    ik_repl_ctx_t *repl = talloc_zero(ctx, ik_repl_ctx_t);
    repl->current = talloc_zero(repl, ik_agent_ctx_t);
    ck_assert_ptr_nonnull(repl);
    repl->shared = shared;
    repl->current = agent;
    // Use the agent's input_buffer (already created by ik_test_create_agent)

    // Initialize reference fields (agent fields are already initialized)
    repl->spinner_state.frame_index = 0;
    repl->spinner_state.visible = false;

    // Initialize state to IDLE
    repl->current->state = IK_AGENT_STATE_IDLE;

    // Create layers
    ik_layer_t *scrollback_layer = ik_scrollback_layer_create(ctx, "scrollback", scrollback);

    ik_layer_t *spinner_layer = ik_spinner_layer_create(ctx, "spinner", &repl->spinner_state);

    ik_layer_t *separator_layer = ik_separator_layer_create(ctx, "separator", &repl->current->separator_visible);

    ik_layer_t *input_layer = ik_input_layer_create(ctx, "input", &repl->current->input_buffer_visible,
                                                    &repl->current->input_text, &repl->current->input_text_len);

    // Add layers to cake
    res = ik_layer_cake_add_layer(layer_cake, scrollback_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(layer_cake, spinner_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(layer_cake, separator_layer);
    ck_assert(is_ok(&res));
    res = ik_layer_cake_add_layer(layer_cake, input_layer);
    ck_assert(is_ok(&res));

    // Create config
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_api_key = talloc_strdup(cfg, "test-api-key");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;
    cfg->openai_system_message = talloc_strdup(cfg, "You are a helpful assistant.");
    // Set config in shared context (already created above)
    shared->cfg = cfg;

    // Create conversation (agent already created above)
    res = ik_openai_conversation_create(ctx);
    ck_assert(is_ok(&res));
    agent->conversation = res.ok;
    repl->current = agent;

    // Create multi handle
    res = ik_openai_multi_create(ctx);
    ck_assert(is_ok(&res));
    repl->current->multi = res.ok;

    // Initialize curl_still_running
    repl->current->curl_still_running = 0;

    // Initialize assistant_response to NULL
    repl->current->assistant_response = NULL;

    return repl;
}
