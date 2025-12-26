/**
 * @file openai.c
 * @brief OpenAI provider implementation
 */

#include "openai.h"
#include "openai_handlers.h"
#include "error.h"
#include "panic.h"
#include "providers/common/http_multi.h"
#include "reasoning.h"
#include "request.h"
#include <assert.h>
#include <string.h>
#include <sys/select.h>

/* ================================================================
 * Forward Declarations - Vtable Methods
 * ================================================================ */

static res_t openai_fdset(void *ctx, fd_set *read_fds, fd_set *write_fds,
                           fd_set *exc_fds, int *max_fd);
static res_t openai_perform(void *ctx, int *running_handles);
static res_t openai_timeout(void *ctx, long *timeout_ms);
static void openai_info_read(void *ctx, ik_logger_t *logger);
static res_t openai_start_request(void *ctx, const ik_request_t *req,
                                   ik_provider_completion_cb_t completion_cb,
                                   void *completion_ctx);
static res_t openai_start_stream(void *ctx, const ik_request_t *req,
                                  ik_stream_cb_t stream_cb, void *stream_ctx,
                                  ik_provider_completion_cb_t completion_cb,
                                  void *completion_ctx);
static void openai_cleanup(void *ctx);
static void openai_cancel(void *ctx);

/* ================================================================
 * Vtable
 * ================================================================ */

static const ik_provider_vtable_t OPENAI_VTABLE = {
    .fdset = openai_fdset,
    .perform = openai_perform,
    .timeout = openai_timeout,
    .info_read = openai_info_read,
    .start_request = openai_start_request,
    .start_stream = openai_start_stream,
    .cleanup = openai_cleanup,
    .cancel = openai_cancel,
};

/* ================================================================
 * Factory Functions
 * ================================================================ */

res_t ik_openai_create(TALLOC_CTX *ctx, const char *api_key, ik_provider_t **out)
{
    return ik_openai_create_with_options(ctx, api_key, false, out);
}

res_t ik_openai_create_with_options(TALLOC_CTX *ctx, const char *api_key,
                                     bool use_responses_api, ik_provider_t **out)
{
    assert(ctx != NULL);     // LCOV_EXCL_BR_LINE
    assert(api_key != NULL); // LCOV_EXCL_BR_LINE
    assert(out != NULL);     // LCOV_EXCL_BR_LINE

    // Validate API key
    if (api_key[0] == '\0') {
        return ERR(ctx, INVALID_ARG, "OpenAI API key cannot be empty");
    }

    // Allocate provider structure
    ik_provider_t *provider = talloc_zero(ctx, ik_provider_t);
    if (provider == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Allocate implementation context as child of provider
    ik_openai_ctx_t *impl_ctx = talloc_zero(provider, ik_openai_ctx_t);
    if (impl_ctx == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Copy API key
    impl_ctx->api_key = talloc_strdup(impl_ctx, api_key);
    if (impl_ctx->api_key == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Set base URL
    impl_ctx->base_url = talloc_strdup(impl_ctx, IK_OPENAI_BASE_URL);
    if (impl_ctx->base_url == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Set API mode
    impl_ctx->use_responses_api = use_responses_api;

    // Create HTTP multi handle
    res_t multi_res = ik_http_multi_create(impl_ctx);
    if (is_err(&multi_res)) {
        talloc_steal(ctx, multi_res.err);
        talloc_free(provider);
        return multi_res;
    }
    impl_ctx->http_multi = multi_res.ok;

    // Initialize provider
    provider->name = "openai";
    provider->vt = &OPENAI_VTABLE;
    provider->ctx = impl_ctx;

    *out = provider;
    return OK(provider);
}

/* ================================================================
 * Vtable Method Implementations (Stubs for Future Tasks)
 * ================================================================ */

static res_t openai_fdset(void *ctx, fd_set *read_fds, fd_set *write_fds,
                           fd_set *exc_fds, int *max_fd)
{
    assert(ctx != NULL);       // LCOV_EXCL_BR_LINE
    assert(read_fds != NULL);  // LCOV_EXCL_BR_LINE
    assert(write_fds != NULL); // LCOV_EXCL_BR_LINE
    assert(exc_fds != NULL);   // LCOV_EXCL_BR_LINE
    assert(max_fd != NULL);    // LCOV_EXCL_BR_LINE

    ik_openai_ctx_t *impl_ctx = (ik_openai_ctx_t *)ctx;
    return ik_http_multi_fdset(impl_ctx->http_multi, read_fds, write_fds, exc_fds, max_fd);
}

static res_t openai_perform(void *ctx, int *running_handles)
{
    assert(ctx != NULL);              // LCOV_EXCL_BR_LINE
    assert(running_handles != NULL);  // LCOV_EXCL_BR_LINE

    ik_openai_ctx_t *impl_ctx = (ik_openai_ctx_t *)ctx;
    return ik_http_multi_perform(impl_ctx->http_multi, running_handles);
}

static res_t openai_timeout(void *ctx, long *timeout_ms)
{
    assert(ctx != NULL);       // LCOV_EXCL_BR_LINE
    assert(timeout_ms != NULL); // LCOV_EXCL_BR_LINE

    ik_openai_ctx_t *impl_ctx = (ik_openai_ctx_t *)ctx;
    return ik_http_multi_timeout(impl_ctx->http_multi, timeout_ms);
}

static void openai_info_read(void *ctx, ik_logger_t *logger)
{
    assert(ctx != NULL);    // LCOV_EXCL_BR_LINE

    ik_openai_ctx_t *impl_ctx = (ik_openai_ctx_t *)ctx;
    ik_http_multi_info_read(impl_ctx->http_multi, logger);
}

/* ================================================================
 * Start Request Implementation
 * ================================================================ */

static res_t openai_start_request(void *ctx, const ik_request_t *req,
                                   ik_provider_completion_cb_t completion_cb,
                                   void *completion_ctx)
{
    assert(ctx != NULL);           // LCOV_EXCL_BR_LINE
    assert(req != NULL);           // LCOV_EXCL_BR_LINE
    assert(completion_cb != NULL); // LCOV_EXCL_BR_LINE

    ik_openai_ctx_t *impl_ctx = (ik_openai_ctx_t *)ctx;

    // Determine which API to use
    bool use_responses_api = impl_ctx->use_responses_api
        || ik_openai_prefer_responses_api(req->model);

    // Create request context for tracking this request
    ik_openai_request_ctx_t *req_ctx = talloc_zero(impl_ctx, ik_openai_request_ctx_t);
    if (req_ctx == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    req_ctx->provider = impl_ctx;
    req_ctx->use_responses_api = use_responses_api;
    req_ctx->cb = completion_cb;
    req_ctx->cb_ctx = completion_ctx;

    // Serialize request to JSON
    char *json_body = NULL;
    res_t serialize_res;

    if (use_responses_api) {
        serialize_res = ik_openai_serialize_responses_request(req_ctx, req, false, &json_body);
    } else {
        serialize_res = ik_openai_serialize_chat_request(req_ctx, req, false, &json_body);
    }

    if (is_err(&serialize_res)) {
        talloc_steal(impl_ctx, serialize_res.err);
        talloc_free(req_ctx);
        return serialize_res;
    }

    // Build URL
    char *url = NULL;
    res_t url_res;

    if (use_responses_api) {
        url_res = ik_openai_build_responses_url(req_ctx, impl_ctx->base_url, &url);
    } else {
        url_res = ik_openai_build_chat_url(req_ctx, impl_ctx->base_url, &url);
    }

    if (is_err(&url_res)) {
        talloc_steal(impl_ctx, url_res.err);
        talloc_free(req_ctx);
        return url_res;
    }

    // Build headers
    char **headers_tmp = NULL;
    res_t headers_res = ik_openai_build_headers(req_ctx, impl_ctx->api_key, &headers_tmp);
    if (is_err(&headers_res)) {
        talloc_steal(impl_ctx, headers_res.err);
        talloc_free(req_ctx);
        return headers_res;
    }

    // Build HTTP request specification
    // Note: headers_tmp is char ** but http_req expects const char **
    // This cast is safe because we're not modifying the pointed-to strings
    // Disable cast-qual warning for this specific cast
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wcast-qual"
    const char **headers_const = (const char **)headers_tmp;
    #pragma GCC diagnostic pop

    ik_http_request_t http_req = {
        .url = url,
        .method = "POST",
        .headers = headers_const,
        .body = json_body,
        .body_len = strlen(json_body)
    };

    // Add request to multi handle
    res_t add_res = ik_http_multi_add_request(impl_ctx->http_multi,
                                               &http_req,
                                               NULL,  // No streaming write callback
                                               NULL,  // No write context
                                               ik_openai_http_completion_handler,
                                               req_ctx);

    if (is_err(&add_res)) {
        talloc_steal(impl_ctx, add_res.err);
        talloc_free(req_ctx);
        return add_res;
    }

    // Request successfully started (returns immediately)
    return OK(NULL);
}

static res_t openai_start_stream(void *ctx, const ik_request_t *req,
                                  ik_stream_cb_t stream_cb, void *stream_ctx,
                                  ik_provider_completion_cb_t completion_cb,
                                  void *completion_ctx)
{
    assert(ctx != NULL);           // LCOV_EXCL_BR_LINE
    assert(req != NULL);           // LCOV_EXCL_BR_LINE
    assert(stream_cb != NULL);     // LCOV_EXCL_BR_LINE
    assert(completion_cb != NULL); // LCOV_EXCL_BR_LINE

    ik_openai_ctx_t *impl_ctx = (ik_openai_ctx_t *)ctx;

    // Determine which API to use
    bool use_responses_api = impl_ctx->use_responses_api
        || ik_openai_prefer_responses_api(req->model);

    // Create streaming request context
    ik_openai_stream_request_ctx_t *req_ctx = talloc_zero(impl_ctx, ik_openai_stream_request_ctx_t);
    if (req_ctx == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    req_ctx->provider = impl_ctx;
    req_ctx->use_responses_api = use_responses_api;
    req_ctx->stream_cb = stream_cb;
    req_ctx->stream_ctx = stream_ctx;
    req_ctx->completion_cb = completion_cb;
    req_ctx->completion_ctx = completion_ctx;
    req_ctx->sse_buffer = NULL;
    req_ctx->sse_buffer_len = 0;

    // Create streaming parser context
    ik_openai_chat_stream_ctx_t *parser_ctx =
        ik_openai_chat_stream_ctx_create(req_ctx, stream_cb, stream_ctx);
    req_ctx->parser_ctx = parser_ctx;

    // Serialize request with stream=true
    char *json_body = NULL;
    res_t serialize_res;

    if (use_responses_api) {
        serialize_res = ik_openai_serialize_responses_request(req_ctx, req, true, &json_body);
    } else {
        serialize_res = ik_openai_serialize_chat_request(req_ctx, req, true, &json_body);
    }

    if (is_err(&serialize_res)) {
        talloc_steal(impl_ctx, serialize_res.err);
        talloc_free(req_ctx);
        return serialize_res;
    }

    // Build URL
    char *url = NULL;
    res_t url_res;

    if (use_responses_api) {
        url_res = ik_openai_build_responses_url(req_ctx, impl_ctx->base_url, &url);
    } else {
        url_res = ik_openai_build_chat_url(req_ctx, impl_ctx->base_url, &url);
    }

    if (is_err(&url_res)) {
        talloc_steal(impl_ctx, url_res.err);
        talloc_free(req_ctx);
        return url_res;
    }

    // Build headers
    char **headers_tmp = NULL;
    res_t headers_res = ik_openai_build_headers(req_ctx, impl_ctx->api_key, &headers_tmp);
    if (is_err(&headers_res)) {
        talloc_steal(impl_ctx, headers_res.err);
        talloc_free(req_ctx);
        return headers_res;
    }

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wcast-qual"
    const char **headers_const = (const char **)headers_tmp;
    #pragma GCC diagnostic pop

    // Build HTTP request specification
    ik_http_request_t http_req = {
        .url = url,
        .method = "POST",
        .headers = headers_const,
        .body = json_body,
        .body_len = strlen(json_body)
    };

    // Add request with streaming write callback
    res_t add_res = ik_http_multi_add_request(
        impl_ctx->http_multi,
        &http_req,
        ik_openai_stream_write_callback,
        req_ctx,
        ik_openai_stream_completion_handler,
        req_ctx);

    if (is_err(&add_res)) {
        talloc_steal(impl_ctx, add_res.err);
        talloc_free(req_ctx);
        return add_res;
    }

    // Request successfully started (returns immediately)
    return OK(NULL);
}

static void openai_cleanup(void *ctx)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE

    (void)ctx;

    // Stub: Cleanup will be implemented when curl_multi is added
    // Currently, talloc handles all cleanup automatically
}

static void openai_cancel(void *ctx)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE

    (void)ctx;

    // Stub: Will be implemented in openai-request.md
    // Must be async-signal-safe (no malloc, no mutex)
}
