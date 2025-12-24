/**
 * @file anthropic.c
 * @brief Anthropic provider implementation
 */

#include "anthropic.h"
#include "thinking.h"
#include "error.h"
#include "response.h"
#include "panic.h"
#include <string.h>
#include <sys/select.h>

/**
 * Anthropic provider implementation context
 */
typedef struct {
    char *api_key;
    char *base_url;
} ik_anthropic_ctx_t;

/* ================================================================
 * Forward Declarations - Vtable Methods
 * ================================================================ */

static res_t anthropic_fdset(void *ctx, fd_set *read_fds, fd_set *write_fds,
                              fd_set *exc_fds, int *max_fd);
static res_t anthropic_perform(void *ctx, int *running_handles);
static res_t anthropic_timeout(void *ctx, long *timeout_ms);
static void anthropic_info_read(void *ctx, ik_logger_t *logger);
static res_t anthropic_start_request(void *ctx, const ik_request_t *req,
                                      ik_provider_completion_cb_t completion_cb,
                                      void *completion_ctx);
static res_t anthropic_start_stream(void *ctx, const ik_request_t *req,
                                     ik_stream_cb_t stream_cb, void *stream_ctx,
                                     ik_provider_completion_cb_t completion_cb,
                                     void *completion_ctx);
static void anthropic_cleanup(void *ctx);
static void anthropic_cancel(void *ctx);

/* ================================================================
 * Vtable
 * ================================================================ */

static const ik_provider_vtable_t ANTHROPIC_VTABLE = {
    .fdset = anthropic_fdset,
    .perform = anthropic_perform,
    .timeout = anthropic_timeout,
    .info_read = anthropic_info_read,
    .start_request = anthropic_start_request,
    .start_stream = anthropic_start_stream,
    .cleanup = anthropic_cleanup,
    .cancel = anthropic_cancel,
};

/* ================================================================
 * Factory Function
 * ================================================================ */

res_t ik_anthropic_create(TALLOC_CTX *ctx, const char *api_key, ik_provider_t **out)
{
    assert(ctx != NULL);     // LCOV_EXCL_BR_LINE
    assert(api_key != NULL); // LCOV_EXCL_BR_LINE
    assert(out != NULL);     // LCOV_EXCL_BR_LINE

    // Allocate provider structure
    ik_provider_t *provider = talloc_zero(ctx, ik_provider_t);
    if (provider == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Allocate implementation context as child of provider
    ik_anthropic_ctx_t *impl_ctx = talloc_zero(provider, ik_anthropic_ctx_t);
    if (impl_ctx == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Copy API key
    impl_ctx->api_key = talloc_strdup(impl_ctx, api_key);
    if (impl_ctx->api_key == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Set base URL
    impl_ctx->base_url = talloc_strdup(impl_ctx, "https://api.anthropic.com");
    if (impl_ctx->base_url == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Initialize provider
    provider->name = "anthropic";
    provider->vt = &ANTHROPIC_VTABLE;
    provider->ctx = impl_ctx;

    *out = provider;
    return OK(provider);
}

/* ================================================================
 * Vtable Method Implementations (Stubs for Future Tasks)
 * ================================================================ */

static res_t anthropic_fdset(void *ctx, fd_set *read_fds, fd_set *write_fds,
                              fd_set *exc_fds, int *max_fd)
{
    assert(ctx != NULL);       // LCOV_EXCL_BR_LINE
    assert(read_fds != NULL);  // LCOV_EXCL_BR_LINE
    assert(write_fds != NULL); // LCOV_EXCL_BR_LINE
    assert(exc_fds != NULL);   // LCOV_EXCL_BR_LINE
    assert(max_fd != NULL);    // LCOV_EXCL_BR_LINE

    (void)ctx;
    (void)read_fds;
    (void)write_fds;
    (void)exc_fds;
    (void)max_fd;

    // Stub: Will be implemented in anthropic-request.md
    return OK(NULL);
}

static res_t anthropic_perform(void *ctx, int *running_handles)
{
    assert(ctx != NULL);              // LCOV_EXCL_BR_LINE
    assert(running_handles != NULL);  // LCOV_EXCL_BR_LINE

    (void)ctx;
    *running_handles = 0;

    // Stub: Will be implemented in anthropic-request.md
    return OK(NULL);
}

static res_t anthropic_timeout(void *ctx, long *timeout_ms)
{
    assert(ctx != NULL);       // LCOV_EXCL_BR_LINE
    assert(timeout_ms != NULL); // LCOV_EXCL_BR_LINE

    (void)ctx;
    *timeout_ms = -1; // No timeout needed (no pending transfers)

    // Stub: Will be implemented in anthropic-request.md
    return OK(NULL);
}

static void anthropic_info_read(void *ctx, ik_logger_t *logger)
{
    assert(ctx != NULL);    // LCOV_EXCL_BR_LINE
    assert(logger != NULL); // LCOV_EXCL_BR_LINE

    (void)ctx;
    (void)logger;

    // Stub: Will be implemented in anthropic-request.md
}

static res_t anthropic_start_request(void *ctx, const ik_request_t *req,
                                      ik_provider_completion_cb_t completion_cb,
                                      void *completion_ctx)
{
    assert(ctx != NULL);           // LCOV_EXCL_BR_LINE
    assert(req != NULL);           // LCOV_EXCL_BR_LINE
    assert(completion_cb != NULL); // LCOV_EXCL_BR_LINE

    // Delegate to response module
    return ik_anthropic_start_request(ctx, req, completion_cb, completion_ctx);
}

static res_t anthropic_start_stream(void *ctx, const ik_request_t *req,
                                     ik_stream_cb_t stream_cb, void *stream_ctx,
                                     ik_provider_completion_cb_t completion_cb,
                                     void *completion_ctx)
{
    assert(ctx != NULL);           // LCOV_EXCL_BR_LINE
    assert(req != NULL);           // LCOV_EXCL_BR_LINE
    assert(stream_cb != NULL);     // LCOV_EXCL_BR_LINE
    assert(completion_cb != NULL); // LCOV_EXCL_BR_LINE

    // Delegate to response module
    return ik_anthropic_start_stream(ctx, req, stream_cb, stream_ctx, completion_cb, completion_ctx);
}

static void anthropic_cleanup(void *ctx)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE

    (void)ctx;

    // Stub: Cleanup will be implemented when curl_multi is added
    // Currently, talloc handles all cleanup automatically
}

static void anthropic_cancel(void *ctx)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE

    (void)ctx;

    // Stub: Will be implemented in anthropic-request.md
    // Must be async-signal-safe (no malloc, no mutex)
}
