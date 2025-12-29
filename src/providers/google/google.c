/**
 * @file google.c
 * @brief Google provider implementation
 */

#include "google.h"

#include "error.h"
#include "google_internal.h"
#include "logger.h"
#include "panic.h"
#include "providers/common/http_multi.h"
#include "providers/common/sse_parser.h"
#include "request.h"
#include "response.h"
#include "streaming.h"
#include "thinking.h"

#include <string.h>
#include <sys/select.h>

/* ================================================================
 * Forward Declarations - Vtable Methods
 * ================================================================ */

static res_t google_fdset(void *ctx, fd_set *read_fds, fd_set *write_fds,
                          fd_set *exc_fds, int *max_fd);
static res_t google_perform(void *ctx, int *running_handles);
static res_t google_timeout(void *ctx, long *timeout_ms);
static void google_info_read(void *ctx, ik_logger_t *logger);
static res_t google_start_request(void *ctx, const ik_request_t *req,
                                   ik_provider_completion_cb_t completion_cb,
                                   void *completion_ctx);
static res_t google_start_stream(void *ctx, const ik_request_t *req,
                                  ik_stream_cb_t stream_cb, void *stream_ctx,
                                  ik_provider_completion_cb_t completion_cb,
                                  void *completion_ctx);
static void google_cleanup(void *ctx);
static void google_cancel(void *ctx);

/* ================================================================
 * Vtable
 * ================================================================ */

static const ik_provider_vtable_t GOOGLE_VTABLE = {
    .fdset = google_fdset,
    .perform = google_perform,
    .timeout = google_timeout,
    .info_read = google_info_read,
    .start_request = google_start_request,
    .start_stream = google_start_stream,
    .cleanup = google_cleanup,
    .cancel = google_cancel,
};

/* ================================================================
 * Factory Function
 * ================================================================ */

res_t ik_google_create(TALLOC_CTX *ctx, const char *api_key, ik_provider_t **out)
{
    assert(ctx != NULL);     // LCOV_EXCL_BR_LINE
    assert(api_key != NULL); // LCOV_EXCL_BR_LINE
    assert(out != NULL);     // LCOV_EXCL_BR_LINE

    // Allocate provider structure
    ik_provider_t *provider = talloc_zero(ctx, ik_provider_t);
    if (provider == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Allocate implementation context as child of provider
    ik_google_ctx_t *impl_ctx = talloc_zero(provider, ik_google_ctx_t);
    if (impl_ctx == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Copy API key
    impl_ctx->api_key = talloc_strdup(impl_ctx, api_key);
    if (impl_ctx->api_key == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Set base URL
    impl_ctx->base_url = talloc_strdup(impl_ctx, "https://generativelanguage.googleapis.com/v1beta");
    if (impl_ctx->base_url == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Create HTTP multi handle for async operations
    res_t r = ik_http_multi_create(impl_ctx);
    if (is_err(&r)) {
        talloc_free(provider);
        return r;
    }
    impl_ctx->http_multi = (ik_http_multi_t *)r.ok;
    impl_ctx->active_stream = NULL;

    // Initialize provider
    provider->name = "google";
    provider->vt = &GOOGLE_VTABLE;
    provider->ctx = impl_ctx;

    *out = provider;
    return OK(provider);
}

/* ================================================================
 * Write Callback for Streaming
 * ================================================================ */

/**
 * HTTP write callback for streaming responses
 *
 * Called by http_multi as data arrives. Feeds data to SSE parser
 * which extracts JSON chunks and processes them through the stream callback.
 */
size_t ik_google_stream_write_cb(const char *data, size_t len, void *ctx)
{
    ik_google_active_stream_t *stream = (ik_google_active_stream_t *)ctx;
    if (stream == NULL || stream->sse_parser == NULL) {
        return len; // Accept data but do nothing
    }

    // Feed data to SSE parser
    ik_sse_parser_feed(stream->sse_parser, data, len);

    // Process all complete events
    ik_sse_event_t *event;
    while ((event = ik_sse_parser_next(stream->sse_parser, stream->stream_ctx)) != NULL) {
        // Google uses newline-delimited JSON, SSE parser extracts data fields
        // The event type is not used for Google (they don't send event: lines)
        const char *event_data = event->data ? event->data : "";

        // Process the chunk through the Google streaming handler
        ik_google_stream_process_data(stream->stream_ctx, event_data);

        // Free the event (allocated on stream_ctx, will be cleaned up automatically)
        talloc_free(event);
    }

    return len;
}

/**
 * HTTP completion callback for streaming
 *
 * Called when the HTTP transfer completes (success or error).
 */
void ik_google_stream_completion_cb(const ik_http_completion_t *completion, void *ctx)
{
    ik_google_active_stream_t *stream = (ik_google_active_stream_t *)ctx;
    if (stream == NULL) {
        return;
    }

    stream->completed = true;
    stream->http_status = completion->http_code;
}

/* ================================================================
 * Vtable Method Implementations
 * ================================================================ */

static res_t google_fdset(void *ctx, fd_set *read_fds, fd_set *write_fds,
                          fd_set *exc_fds, int *max_fd)
{
    assert(ctx != NULL);       // LCOV_EXCL_BR_LINE
    assert(read_fds != NULL);  // LCOV_EXCL_BR_LINE
    assert(write_fds != NULL); // LCOV_EXCL_BR_LINE
    assert(exc_fds != NULL);   // LCOV_EXCL_BR_LINE
    assert(max_fd != NULL);    // LCOV_EXCL_BR_LINE

    ik_google_ctx_t *impl_ctx = (ik_google_ctx_t *)ctx;
    return ik_http_multi_fdset(impl_ctx->http_multi, read_fds, write_fds, exc_fds, max_fd);
}

static res_t google_perform(void *ctx, int *running_handles)
{
    assert(ctx != NULL);              // LCOV_EXCL_BR_LINE
    assert(running_handles != NULL);  // LCOV_EXCL_BR_LINE

    ik_google_ctx_t *impl_ctx = (ik_google_ctx_t *)ctx;
    return ik_http_multi_perform(impl_ctx->http_multi, running_handles);
}

static res_t google_timeout(void *ctx, long *timeout_ms)
{
    assert(ctx != NULL);       // LCOV_EXCL_BR_LINE
    assert(timeout_ms != NULL); // LCOV_EXCL_BR_LINE

    ik_google_ctx_t *impl_ctx = (ik_google_ctx_t *)ctx;
    return ik_http_multi_timeout(impl_ctx->http_multi, timeout_ms);
}

static void google_info_read(void *ctx, ik_logger_t *logger)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE

    ik_google_ctx_t *impl_ctx = (ik_google_ctx_t *)ctx;

    // Process completed transfers
    ik_http_multi_info_read(impl_ctx->http_multi, logger);

    // Check if streaming is complete and invoke completion callback
    if (impl_ctx->active_stream != NULL && impl_ctx->active_stream->completed) {
        ik_google_active_stream_t *stream = impl_ctx->active_stream;

        // Build completion info
        ik_provider_completion_t completion = {0};

        if (stream->http_status >= 200 && stream->http_status < 300) {
            completion.success = true;
            completion.http_status = stream->http_status;
            completion.response = NULL; // Streaming doesn't build full response
            completion.error_category = IK_ERR_CAT_UNKNOWN;
            completion.error_message = NULL;
            completion.retry_after_ms = -1;
        } else {
            completion.success = false;
            completion.http_status = stream->http_status;
            completion.response = NULL;

            // Map HTTP status to error category
            if (stream->http_status == 401 || stream->http_status == 403) {
                completion.error_category = IK_ERR_CAT_AUTH;
            } else if (stream->http_status == 429) {
                completion.error_category = IK_ERR_CAT_RATE_LIMIT;
            } else if (stream->http_status >= 500) {
                completion.error_category = IK_ERR_CAT_SERVER;
            } else {
                completion.error_category = IK_ERR_CAT_UNKNOWN;
            }

            completion.error_message = talloc_asprintf(impl_ctx, "HTTP %d", stream->http_status);
            completion.retry_after_ms = -1;
        }

        // Invoke completion callback
        if (stream->completion_cb != NULL) {
            stream->completion_cb(&completion, stream->completion_ctx);
        }

        // Clean up
        if (completion.error_message != NULL) {
            talloc_free(completion.error_message);
        }

        talloc_free(impl_ctx->active_stream);
        impl_ctx->active_stream = NULL;
    }
}

static res_t google_start_request(void *ctx, const ik_request_t *req,
                                   ik_provider_completion_cb_t completion_cb,
                                   void *completion_ctx)
{
    assert(ctx != NULL);           // LCOV_EXCL_BR_LINE
    assert(req != NULL);           // LCOV_EXCL_BR_LINE
    assert(completion_cb != NULL); // LCOV_EXCL_BR_LINE

    // Delegate to response module (non-streaming)
    return ik_google_start_request(ctx, req, completion_cb, completion_ctx);
}

static res_t google_start_stream(void *ctx, const ik_request_t *req,
                                  ik_stream_cb_t stream_cb, void *stream_ctx,
                                  ik_provider_completion_cb_t completion_cb,
                                  void *completion_ctx)
{
    assert(ctx != NULL);           // LCOV_EXCL_BR_LINE
    assert(req != NULL);           // LCOV_EXCL_BR_LINE
    assert(stream_cb != NULL);     // LCOV_EXCL_BR_LINE
    assert(completion_cb != NULL); // LCOV_EXCL_BR_LINE

    ik_google_ctx_t *impl_ctx = (ik_google_ctx_t *)ctx;

    // Create active stream context
    ik_google_active_stream_t *active_stream = talloc_zero(impl_ctx, ik_google_active_stream_t);
    if (active_stream == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Create streaming context for event processing
    res_t r = ik_google_stream_ctx_create(active_stream, stream_cb, stream_ctx,
                                           &active_stream->stream_ctx);
    if (is_err(&r)) {
        talloc_free(active_stream);
        return r;
    }

    // Create SSE parser
    active_stream->sse_parser = ik_sse_parser_create(active_stream);
    active_stream->completion_cb = completion_cb;
    active_stream->completion_ctx = completion_ctx;
    active_stream->completed = false;
    active_stream->http_status = 0;

    // Build URL with streaming parameters
    char *url = NULL;
    r = ik_google_build_url(active_stream, impl_ctx->base_url, req->model,
                            impl_ctx->api_key, true, &url);
    if (is_err(&r)) {
        talloc_free(active_stream);
        return r;
    }

    // Serialize request JSON
    char *body = NULL;
    r = ik_google_serialize_request(active_stream, req, &body);
    if (is_err(&r)) {
        talloc_free(active_stream);
        return r;
    }

    // Build headers (Google uses API key in URL, so headers are simpler)
    const char *headers[3];
    headers[0] = "Content-Type: application/json";
    headers[1] = "Accept: text/event-stream";
    headers[2] = NULL;

    // Build HTTP request
    ik_http_request_t http_req = {
        .url = url,
        .method = "POST",
        .headers = headers,
        .body = body,
        .body_len = strlen(body)
    };

    // Store active stream
    impl_ctx->active_stream = active_stream;

    // Add request to http_multi
    r = ik_http_multi_add_request(impl_ctx->http_multi, &http_req,
                                   ik_google_stream_write_cb, active_stream,
                                   ik_google_stream_completion_cb, active_stream);
    if (is_err(&r)) {
        impl_ctx->active_stream = NULL;
        talloc_free(active_stream);
        return r;
    }

    return OK(NULL);
}

static void google_cleanup(void *ctx)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE

    // talloc handles cleanup automatically since http_multi is a child of impl_ctx
    (void)ctx;
}

static void google_cancel(void *ctx)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE

    ik_google_ctx_t *impl_ctx = (ik_google_ctx_t *)ctx;

    // Clear active stream (no malloc in signal handler)
    if (impl_ctx->active_stream != NULL) {
        impl_ctx->active_stream->completed = true;
    }
}
