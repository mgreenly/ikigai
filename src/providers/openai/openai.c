/**
 * @file openai.c
 * @brief OpenAI provider implementation
 */

#include "openai.h"
#include "reasoning.h"
#include "request.h"
#include "response.h"
#include "streaming.h"
#include "error.h"
#include "panic.h"
#include "providers/common/http_multi.h"
#include <string.h>
#include <sys/select.h>
#include <assert.h>

/**
 * OpenAI provider implementation context
 */
typedef struct {
    char *api_key;
    char *base_url;
    bool use_responses_api;
    ik_http_multi_t *http_multi;
} ik_openai_ctx_t;

/**
 * Internal request context for tracking in-flight requests
 */
typedef struct {
    ik_openai_ctx_t *provider;
    bool use_responses_api;
    ik_provider_completion_cb_t cb;
    void *cb_ctx;
} ik_openai_request_ctx_t;

/**
 * Internal request context for tracking in-flight streaming requests
 */
typedef struct {
    ik_openai_ctx_t *provider;
    bool use_responses_api;
    ik_stream_cb_t stream_cb;
    void *stream_ctx;
    ik_provider_completion_cb_t completion_cb;
    void *completion_ctx;
    ik_openai_chat_stream_ctx_t *parser_ctx;
    char *sse_buffer;
    size_t sse_buffer_len;
} ik_openai_stream_request_ctx_t;

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
 * HTTP Completion Callback
 * ================================================================ */

/**
 * HTTP completion callback for non-streaming requests
 *
 * Called from info_read() when HTTP transfer completes.
 * Parses response and invokes user's completion callback.
 */
static void http_completion_handler(const ik_http_completion_t *http_completion, void *user_ctx)
{
    ik_openai_request_ctx_t *req_ctx = (ik_openai_request_ctx_t *)user_ctx;
    assert(req_ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(req_ctx->cb != NULL);  // LCOV_EXCL_BR_LINE

    // Build provider completion structure
    ik_provider_completion_t provider_completion = {0};
    provider_completion.http_status = http_completion->http_code;

    // Handle HTTP errors
    if (http_completion->type != IK_HTTP_SUCCESS) {
        provider_completion.success = false;
        provider_completion.response = NULL;
        provider_completion.retry_after_ms = -1;

        // Parse error response if we have JSON body
        if (http_completion->response_body != NULL && http_completion->response_len > 0) {
            ik_error_category_t category;
            char *error_msg = NULL;
            res_t parse_res = ik_openai_parse_error(req_ctx, http_completion->http_code,
                                                     http_completion->response_body,
                                                     http_completion->response_len,
                                                     &category, &error_msg);
            if (is_ok(&parse_res)) {
                provider_completion.error_category = category;
                provider_completion.error_message = error_msg;
            } else {
                // Fallback to generic error message
                provider_completion.error_category = IK_ERR_CAT_UNKNOWN;
                provider_completion.error_message = talloc_asprintf(req_ctx,
                    "HTTP %d error", http_completion->http_code);
            }
        } else {
            // Network error or no response body
            if (http_completion->http_code == 0) {
                provider_completion.error_category = IK_ERR_CAT_NETWORK;
            } else {
                provider_completion.error_category = IK_ERR_CAT_UNKNOWN;
            }
            provider_completion.error_message = http_completion->error_message != NULL
                ? talloc_strdup(req_ctx, http_completion->error_message)
                : talloc_asprintf(req_ctx, "HTTP %d error", http_completion->http_code);
        }

        // Invoke user callback with error
        req_ctx->cb(&provider_completion, req_ctx->cb_ctx);

        // Cleanup
        talloc_free(req_ctx);
        return;
    }

    // Parse successful response
    ik_response_t *response = NULL;
    res_t parse_res;

    if (req_ctx->use_responses_api) {
        parse_res = ik_openai_parse_responses_response(req_ctx,
                                                        http_completion->response_body,
                                                        http_completion->response_len,
                                                        &response);
    } else {
        parse_res = ik_openai_parse_chat_response(req_ctx,
                                                   http_completion->response_body,
                                                   http_completion->response_len,
                                                   &response);
    }

    if (is_err(&parse_res)) {
        // Parse error
        provider_completion.success = false;
        provider_completion.response = NULL;
        provider_completion.error_category = IK_ERR_CAT_UNKNOWN;
        provider_completion.error_message = talloc_asprintf(req_ctx,
            "Failed to parse response: %s", parse_res.err->msg);
        provider_completion.retry_after_ms = -1;

        // Invoke user callback with error
        req_ctx->cb(&provider_completion, req_ctx->cb_ctx);

        // Cleanup
        talloc_free(req_ctx);
        return;
    }

    // Success - invoke callback with parsed response
    provider_completion.success = true;
    provider_completion.response = response;
    provider_completion.error_category = IK_ERR_CAT_UNKNOWN;  // Not used on success
    provider_completion.error_message = NULL;
    provider_completion.retry_after_ms = -1;

    req_ctx->cb(&provider_completion, req_ctx->cb_ctx);

    // Cleanup request context
    talloc_free(req_ctx);
}

/* ================================================================
 * Streaming Callbacks
 * ================================================================ */

/**
 * curl write callback for SSE streaming
 *
 * Called during perform() as HTTP chunks arrive.
 * Parses SSE format and feeds data events to parser.
 */
static size_t openai_stream_write_callback(const char *data, size_t len, void *userdata)
{
    ik_openai_stream_request_ctx_t *req_ctx = (ik_openai_stream_request_ctx_t *)userdata;
    assert(req_ctx != NULL);  // LCOV_EXCL_BR_LINE

    // Append to buffer (handles incomplete lines across chunks)
    char *new_buffer = talloc_realloc(req_ctx, req_ctx->sse_buffer,
                                       char, (unsigned int)(req_ctx->sse_buffer_len + len + 1));
    if (new_buffer == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    memcpy(new_buffer + req_ctx->sse_buffer_len, data, len);
    req_ctx->sse_buffer_len += len;
    new_buffer[req_ctx->sse_buffer_len] = '\0';
    req_ctx->sse_buffer = new_buffer;

    // Process complete lines
    char *line_start = req_ctx->sse_buffer;
    char *line_end;

    while ((line_end = strchr(line_start, '\n')) != NULL) {
        *line_end = '\0';

        // Check for "data: " prefix
        if (strncmp(line_start, "data: ", 6) == 0) {
            const char *json_data = line_start + 6;

            // Feed to parser - this invokes user's stream_cb
            ik_openai_chat_stream_process_data(req_ctx->parser_ctx, json_data);
        }

        // Move to next line
        line_start = line_end + 1;
    }

    // Keep incomplete line in buffer
    size_t remaining = req_ctx->sse_buffer_len - (size_t)(line_start - req_ctx->sse_buffer);
    if (remaining > 0) {
        memmove(req_ctx->sse_buffer, line_start, remaining);
        req_ctx->sse_buffer_len = remaining;
        req_ctx->sse_buffer[remaining] = '\0';
    } else {
        talloc_free(req_ctx->sse_buffer);
        req_ctx->sse_buffer = NULL;
        req_ctx->sse_buffer_len = 0;
    }

    return len;
}

/**
 * HTTP completion callback for streaming requests
 *
 * Called from info_read() when HTTP transfer completes.
 * Invokes user's completion callback with final metadata.
 */
static void openai_stream_completion_handler(const ik_http_completion_t *http_completion,
                                               void *user_ctx)
{
    ik_openai_stream_request_ctx_t *req_ctx = (ik_openai_stream_request_ctx_t *)user_ctx;
    assert(req_ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(req_ctx->completion_cb != NULL);  // LCOV_EXCL_BR_LINE

    // Build provider completion structure
    ik_provider_completion_t provider_completion = {0};
    provider_completion.http_status = http_completion->http_code;

    // Handle HTTP errors
    if (http_completion->type != IK_HTTP_SUCCESS) {
        provider_completion.success = false;
        provider_completion.response = NULL;
        provider_completion.retry_after_ms = -1;

        // Parse error if JSON body available
        if (http_completion->response_body != NULL && http_completion->response_len > 0) {
            ik_error_category_t category;
            char *error_msg = NULL;
            res_t parse_res = ik_openai_parse_error(req_ctx, http_completion->http_code,
                                                     http_completion->response_body,
                                                     http_completion->response_len,
                                                     &category, &error_msg);
            if (is_ok(&parse_res)) {
                provider_completion.error_category = category;
                provider_completion.error_message = error_msg;
            } else {
                provider_completion.error_category = IK_ERR_CAT_UNKNOWN;
                provider_completion.error_message = talloc_asprintf(req_ctx,
                    "HTTP %d error", http_completion->http_code);
            }
        } else {
            if (http_completion->http_code == 0) {
                provider_completion.error_category = IK_ERR_CAT_NETWORK;
            } else {
                provider_completion.error_category = IK_ERR_CAT_UNKNOWN;
            }
            provider_completion.error_message = http_completion->error_message != NULL
                ? talloc_strdup(req_ctx, http_completion->error_message)
                : talloc_asprintf(req_ctx, "HTTP %d error", http_completion->http_code);
        }

        req_ctx->completion_cb(&provider_completion, req_ctx->completion_ctx);
        talloc_free(req_ctx);
        return;
    }

    // Success - stream events were already delivered during perform()
    provider_completion.success = true;
    provider_completion.response = NULL;
    provider_completion.error_category = IK_ERR_CAT_UNKNOWN;
    provider_completion.error_message = NULL;
    provider_completion.retry_after_ms = -1;

    req_ctx->completion_cb(&provider_completion, req_ctx->completion_ctx);

    // Cleanup
    talloc_free(req_ctx);
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
                                               http_completion_handler,
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
        openai_stream_write_callback,
        req_ctx,
        openai_stream_completion_handler,
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
