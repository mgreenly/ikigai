/**
 * @file openai.c
 * @brief OpenAI provider implementation
 */

#include "openai.h"
#include "reasoning.h"
#include "error.h"
#include "panic.h"
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
} ik_openai_ctx_t;

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

    (void)ctx;
    (void)read_fds;
    (void)write_fds;
    (void)exc_fds;
    (void)max_fd;

    // Stub: Will be implemented in openai-request.md
    return OK(NULL);
}

static res_t openai_perform(void *ctx, int *running_handles)
{
    assert(ctx != NULL);              // LCOV_EXCL_BR_LINE
    assert(running_handles != NULL);  // LCOV_EXCL_BR_LINE

    (void)ctx;
    *running_handles = 0;

    // Stub: Will be implemented in openai-request.md
    return OK(NULL);
}

static res_t openai_timeout(void *ctx, long *timeout_ms)
{
    assert(ctx != NULL);       // LCOV_EXCL_BR_LINE
    assert(timeout_ms != NULL); // LCOV_EXCL_BR_LINE

    (void)ctx;
    *timeout_ms = -1; // No timeout needed (no pending transfers)

    // Stub: Will be implemented in openai-request.md
    return OK(NULL);
}

static void openai_info_read(void *ctx, ik_logger_t *logger)
{
    assert(ctx != NULL);    // LCOV_EXCL_BR_LINE

    (void)ctx;
    (void)logger;

    // Stub: Will be implemented in openai-request.md
    // Note: logger may be NULL during testing
}

static res_t openai_start_request(void *ctx, const ik_request_t *req,
                                   ik_provider_completion_cb_t completion_cb,
                                   void *completion_ctx)
{
    assert(ctx != NULL);           // LCOV_EXCL_BR_LINE
    assert(req != NULL);           // LCOV_EXCL_BR_LINE
    assert(completion_cb != NULL); // LCOV_EXCL_BR_LINE

    (void)ctx;
    (void)req;
    (void)completion_cb;
    (void)completion_ctx;

    // Stub: Will be implemented in openai-request.md
    TALLOC_CTX *tmp = talloc_new(NULL);
    res_t result = ERR(tmp, NOT_IMPLEMENTED, "openai_start_request not yet implemented");
    talloc_free(tmp);
    return result;
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

    (void)ctx;
    (void)req;
    (void)stream_cb;
    (void)stream_ctx;
    (void)completion_cb;
    (void)completion_ctx;

    // Stub: Will be implemented in openai-streaming.md
    TALLOC_CTX *tmp = talloc_new(NULL);
    res_t result = ERR(tmp, NOT_IMPLEMENTED, "openai_start_stream not yet implemented");
    talloc_free(tmp);
    return result;
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
