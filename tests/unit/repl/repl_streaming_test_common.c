#include "agent.h"
/**
 * @file repl_streaming_test_common.c
 * @brief Common mock infrastructure implementation for streaming tests
 */

#include "repl_streaming_test_common.h"
#include "../../../src/agent.h"
#include "../../../src/providers/provider.h"
#include "../../../src/repl_callbacks.h"
#include "../../../src/shared.h"
#include "../../../src/config.h"
#include "../../../src/openai/client.h"
#include <stdlib.h>

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

// Vtable callbacks that wrap the multi handle for test provider
static res_t test_vt_fdset(void *pctx, fd_set *read_fds, fd_set *write_fds, fd_set *exc_fds, int *max_fd) {
    test_provider_ctx_t *tctx = (test_provider_ctx_t *)pctx;
    return ik_openai_multi_fdset(tctx->multi, read_fds, write_fds, exc_fds, max_fd);
}
static res_t test_vt_perform(void *pctx, int *running_handles) {
    test_provider_ctx_t *tctx = (test_provider_ctx_t *)pctx;
    return ik_openai_multi_perform(tctx->multi, running_handles);
}
static res_t test_vt_timeout(void *pctx, long *timeout_ms) {
    test_provider_ctx_t *tctx = (test_provider_ctx_t *)pctx;
    return ik_openai_multi_timeout(tctx->multi, timeout_ms);
}
static void test_vt_info_read(void *pctx, ik_logger_t *logger) {
    test_provider_ctx_t *tctx = (test_provider_ctx_t *)pctx;
    ik_openai_multi_info_read(tctx->multi, logger);
}

// Context for callback adapters
typedef struct {
    ik_stream_cb_t provider_stream_cb;
    void *provider_stream_ctx;
    ik_provider_completion_cb_t provider_completion_cb;
    void *provider_completion_ctx;
} callback_adapter_ctx_t;

// Adapter: old OpenAI chunk callback → new provider stream callback
static res_t streaming_callback_adapter(const char *chunk, void *ctx)
{
    callback_adapter_ctx_t *adapter = (callback_adapter_ctx_t *)ctx;

    // Convert chunk to provider stream event
    ik_stream_event_t event = {
        .type = IK_STREAM_TEXT_DELTA,
        .index = 0,
        .data = {
            .delta = {
                .text = chunk
            }
        }
    };

    // Invoke provider callback
    return adapter->provider_stream_cb(&event, adapter->provider_stream_ctx);
}

// Adapter: old HTTP completion callback → new provider completion callback
static res_t completion_callback_adapter(const ik_http_completion_t *http_completion, void *ctx)
{
    callback_adapter_ctx_t *adapter = (callback_adapter_ctx_t *)ctx;

    // Convert HTTP completion to provider completion
    ik_provider_completion_t provider_completion = {
        .success = (http_completion->type == IK_HTTP_SUCCESS),
        .http_status = http_completion->http_code,
        .response = NULL,  // Streaming doesn't provide full response in completion
        .error_category = IK_ERR_CAT_UNKNOWN,
        .error_message = http_completion->error_message,
        .retry_after_ms = -1
    };

    // Invoke provider callback
    return adapter->provider_completion_cb(&provider_completion, adapter->provider_completion_ctx);
}

// Mock start_stream for streaming tests - adds request to multi handle
static res_t test_vt_start_stream(void *pctx, const ik_request_t *req,
                                   ik_stream_cb_t stream_cb, void *stream_ctx,
                                   ik_provider_completion_cb_t completion_cb, void *completion_ctx) {
    (void)req;  // Request content not used in mock (uses conversation from agent)

    test_provider_ctx_t *tctx = (test_provider_ctx_t *)pctx;

    // Create adapter context (allocated on multi handle so it persists until completion)
    callback_adapter_ctx_t *adapter = talloc_zero(tctx->multi, callback_adapter_ctx_t);
    if (adapter == NULL) {
        return ERR(tctx->multi, OUT_OF_MEMORY, "Failed to allocate callback adapter");
    }

    adapter->provider_stream_cb = stream_cb;
    adapter->provider_stream_ctx = stream_ctx;
    adapter->provider_completion_cb = completion_cb;
    adapter->provider_completion_ctx = completion_ctx;

    // Get agent from stream context (assumes stream_ctx is ik_agent_ctx_t)
    ik_agent_ctx_t *agent = (ik_agent_ctx_t *)stream_ctx;

    // Create minimal config for multi_add_request
    ik_config_t *cfg = talloc_zero(adapter, ik_config_t);
    cfg->openai_model = talloc_strdup(cfg, agent->model ? agent->model : "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;
    cfg->openai_system_message = NULL;

    // Add request to multi handle using old OpenAI API
    return ik_openai_multi_add_request(
        tctx->multi,
        cfg,
        agent->conversation,
        streaming_callback_adapter,
        adapter,
        completion_callback_adapter,
        adapter,
        false,  // limit_reached
        NULL    // logger
    );
}

static const ik_provider_vtable_t g_test_vtable = {
    .fdset = test_vt_fdset, .perform = test_vt_perform, .timeout = test_vt_timeout,
    .info_read = test_vt_info_read, .start_request = NULL, .start_stream = test_vt_start_stream,
    .cleanup = NULL, .cancel = NULL,
};

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

    // Update agent's shared pointer to use the REPL's shared context (which has cfg)
    agent->shared = shared;

    // Set provider and model so ik_agent_get_provider doesn't fail
    agent->provider = talloc_strdup(agent, "openai");
    agent->model = talloc_strdup(agent, "gpt-4");
    // Use the agent's input_buffer (already created by ik_test_create_agent)

    // Initialize reference fields (agent fields are already initialized)
    repl->current->spinner_state.frame_index = 0;
    repl->current->spinner_state.visible = false;

    // Initialize state to IDLE
    repl->current->state = IK_AGENT_STATE_IDLE;

    // Create layers
    ik_layer_t *scrollback_layer = ik_scrollback_layer_create(ctx, "scrollback", scrollback);

    ik_layer_t *spinner_layer = ik_spinner_layer_create(ctx, "spinner", &repl->current->spinner_state);

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
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    ck_assert_ptr_nonnull(cfg);
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;
    cfg->openai_system_message = talloc_strdup(cfg, "You are a helpful assistant.");
    // Set config in shared context (already created above)
    shared->cfg = cfg;

    // Create logger (required for provider operations)
    shared->logger = ik_logger_create(shared, "/tmp");
    ck_assert_ptr_nonnull(shared->logger);

    // Create conversation (agent already created above)
    agent->conversation = ik_openai_conversation_create(ctx);
    repl->current = agent;

    // Create multi handle wrapped in a mock provider
    res = ik_openai_multi_create(ctx);
    ck_assert(is_ok(&res));

    // Create mock provider for tests that need direct multi access
    // Uses test_provider_ctx_t from header for compatibility with TEST_GET_MULTI macro
    test_provider_ctx_t *mock_ctx = talloc_zero(repl->current, test_provider_ctx_t);
    mock_ctx->multi = res.ok;

    ik_provider_t *provider = talloc_zero(repl->current, ik_provider_t);
    provider->name = "test";
    provider->vt = &g_test_vtable;
    provider->ctx = mock_ctx;
    repl->current->provider_instance = provider;

    // Initialize curl_still_running
    repl->current->curl_still_running = 0;

    // Initialize assistant_response to NULL
    repl->current->assistant_response = NULL;

    return repl;
}
