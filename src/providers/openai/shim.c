#include "providers/openai/shim.h"
#include "panic.h"
#include <string.h>

/* External declarations to avoid header conflict between provider.h and openai/tool_choice.h */
extern res_t ik_openai_multi_create(void *parent);
extern res_t ik_openai_multi_fdset(ik_openai_multi_t *multi, fd_set *read_fds, fd_set *write_fds, fd_set *exc_fds, int *max_fd);
extern res_t ik_openai_multi_perform(ik_openai_multi_t *multi, int *still_running);
extern res_t ik_openai_multi_timeout(ik_openai_multi_t *multi, long *timeout_ms);
extern void ik_openai_multi_info_read(ik_openai_multi_t *multi, ik_logger_t *logger);

/* ================================================================
 * Vtable Methods - Event Loop Integration
 *
 * These methods forward to the existing OpenAI multi-handle implementation.
 * ================================================================ */

static res_t openai_fdset(void *ctx, fd_set *read_fds, fd_set *write_fds, fd_set *exc_fds, int *max_fd)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    ik_openai_shim_ctx_t *shim = (ik_openai_shim_ctx_t *)ctx;
    return ik_openai_multi_fdset(shim->multi, read_fds, write_fds, exc_fds, max_fd);
}

static res_t openai_perform(void *ctx, int *running_handles)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    ik_openai_shim_ctx_t *shim = (ik_openai_shim_ctx_t *)ctx;
    return ik_openai_multi_perform(shim->multi, running_handles);
}

static res_t openai_timeout(void *ctx, long *timeout_ms)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    ik_openai_shim_ctx_t *shim = (ik_openai_shim_ctx_t *)ctx;
    return ik_openai_multi_timeout(shim->multi, timeout_ms);
}

static void openai_info_read(void *ctx, ik_logger_t *logger)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    ik_openai_shim_ctx_t *shim = (ik_openai_shim_ctx_t *)ctx;
    ik_openai_multi_info_read(shim->multi, logger);
}

static res_t openai_start_request(void *ctx, const ik_request_t *req,
                                   ik_provider_completion_cb_t completion_cb, void *completion_ctx)
{
    (void)ctx;
    (void)req;
    (void)completion_cb;
    (void)completion_ctx;

    /* LCOV_EXCL_START */
    TALLOC_CTX *tmp = talloc_new(NULL);
    res_t result = ERR(tmp, NOT_IMPLEMENTED, "openai_start_request not yet implemented");
    talloc_free(tmp);
    return result;
    /* LCOV_EXCL_STOP */
}

static res_t openai_start_stream(void *ctx, const ik_request_t *req,
                                  ik_stream_cb_t stream_cb, void *stream_ctx,
                                  ik_provider_completion_cb_t completion_cb, void *completion_ctx)
{
    (void)ctx;
    (void)req;
    (void)stream_cb;
    (void)stream_ctx;
    (void)completion_cb;
    (void)completion_ctx;

    /* LCOV_EXCL_START */
    TALLOC_CTX *tmp = talloc_new(NULL);
    res_t result = ERR(tmp, NOT_IMPLEMENTED, "openai_start_stream not yet implemented");
    talloc_free(tmp);
    return result;
    /* LCOV_EXCL_STOP */
}

static void openai_cleanup(void *ctx)
{
    (void)ctx;

    /* LCOV_EXCL_START */
    /* Stub: talloc hierarchy handles cleanup */
    /* LCOV_EXCL_STOP */
}

static void openai_cancel(void *ctx)
{
    (void)ctx;

    /* LCOV_EXCL_START */
    /* Stub: cancellation not yet implemented */
    /* LCOV_EXCL_STOP */
}

/* ================================================================
 * OpenAI Provider Vtable
 * ================================================================ */

static const ik_provider_vtable_t openai_vtable = {
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
 * Public API Implementation
 * ================================================================ */

res_t ik_openai_create(TALLOC_CTX *ctx, const char *api_key, ik_provider_t **out)
{
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(out != NULL);   // LCOV_EXCL_BR_LINE

    /* Validate API key */
    if (api_key == NULL) {
        return ERR(ctx, MISSING_CREDENTIALS, "OpenAI API key is NULL");
    }

    /* Allocate provider */
    ik_provider_t *provider = talloc_zero(ctx, ik_provider_t);
    if (!provider) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    /* Allocate shim context as child of provider */
    ik_openai_shim_ctx_t *shim = talloc_zero(provider, ik_openai_shim_ctx_t);
    if (!shim) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    /* Duplicate API key as child of shim context */
    shim->api_key = talloc_strdup(shim, api_key);
    if (!shim->api_key) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    /* Create multi-handle for async HTTP */
    res_t multi_res = ik_openai_multi_create(shim);
    if (is_err(&multi_res)) {
        /* Error is on shim context, need to reparent before cleanup */
        talloc_steal(ctx, multi_res.err);
        talloc_free(provider);
        return multi_res;
    }
    shim->multi = multi_res.ok;

    /* Initialize provider with vtable */
    provider->name = "openai";
    provider->vt = &openai_vtable;
    provider->ctx = shim;

    *out = provider;
    return OK(provider);
}

void ik_openai_shim_destroy(void *impl_ctx)
{
    /* NULL-safe: no-op if context is NULL */
    if (impl_ctx == NULL) {
        return;
    }

    /* All cleanup handled by talloc hierarchy */
    /* This function exists for API symmetry and future needs */
}
