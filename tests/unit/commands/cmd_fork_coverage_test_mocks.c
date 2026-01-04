/**
 * @file cmd_fork_coverage_test_mocks.c
 * @brief Mock functions for cmd_fork_coverage_test
 */

#include "cmd_fork_coverage_test_mocks.h"

#include "../../../src/agent.h"
#include "../../../src/error.h"
#include "../../../src/providers/provider.h"
#include "../../../src/providers/provider_vtable.h"
#include "../../../src/providers/request.h"
#include "../../../src/wrapper.h"

#include <talloc.h>

res_t mock_start_stream(void *ctx,
                        const ik_request_t *req,
                        ik_stream_cb_t stream_cb,
                        void *stream_ctx,
                        ik_provider_completion_cb_t completion_cb,
                        void *completion_ctx);

// Mock control flags
static bool mock_get_provider_should_fail = false;
static bool mock_build_request_should_fail = false;
static bool mock_start_stream_should_fail = false;
static TALLOC_CTX *mock_err_ctx = NULL;

// Mock posix_rename_ to prevent PANIC during logger rotation
int posix_rename_(const char *oldpath, const char *newpath)
{
    (void)oldpath;
    (void)newpath;
    return 0;
}

// Mock ik_agent_get_provider_
res_t ik_agent_get_provider_(void *agent, void **provider_out)
{
    if (mock_get_provider_should_fail) {
        if (mock_err_ctx == NULL) mock_err_ctx = talloc_new(NULL);
        return ERR(mock_err_ctx, PROVIDER, "Mock provider error: Failed to get provider");
    }

    ik_agent_ctx_t *ag = (ik_agent_ctx_t *)agent;

    // Lazily create provider_instance if NULL (mimics real behavior)
    if (ag->provider_instance == NULL) {
        ik_provider_t *provider = talloc_zero(ag, ik_provider_t);
        if (provider == NULL) {
            if (mock_err_ctx == NULL) mock_err_ctx = talloc_new(NULL);
            return ERR(mock_err_ctx, OUT_OF_MEMORY, "Out of memory");
        }
        provider->ctx = ag;

        ik_provider_vtable_t *vt = talloc_zero(provider, ik_provider_vtable_t);
        if (vt == NULL) {
            if (mock_err_ctx == NULL) mock_err_ctx = talloc_new(NULL);
            return ERR(mock_err_ctx, OUT_OF_MEMORY, "Out of memory");
        }
        vt->start_stream = mock_start_stream;
        provider->vt = vt;
        ag->provider_instance = provider;
    }

    *provider_out = ag->provider_instance;
    return OK(NULL);
}

// Mock ik_request_build_from_conversation_
res_t ik_request_build_from_conversation_(TALLOC_CTX *ctx, void *agent, void **req_out)
{
    (void)agent;

    if (mock_build_request_should_fail) {
        if (mock_err_ctx == NULL) mock_err_ctx = talloc_new(NULL);
        return ERR(mock_err_ctx, INVALID_ARG, "Mock request error: Failed to build request");
    }

    ik_request_t *req = talloc_zero(ctx, ik_request_t);
    if (req == NULL) {
        if (mock_err_ctx == NULL) mock_err_ctx = talloc_new(NULL);
        return ERR(mock_err_ctx, OUT_OF_MEMORY, "Out of memory");
    }
    *req_out = req;
    return OK(NULL);
}

// Mock start_stream function for provider
res_t mock_start_stream(void *ctx, const ik_request_t *req,
                        ik_stream_cb_t stream_cb, void *stream_ctx,
                        ik_provider_completion_cb_t completion_cb, void *completion_ctx)
{
    (void)ctx;
    (void)req;
    (void)stream_cb;
    (void)stream_ctx;
    (void)completion_cb;
    (void)completion_ctx;

    if (mock_start_stream_should_fail) {
        if (mock_err_ctx == NULL) mock_err_ctx = talloc_new(NULL);
        return ERR(mock_err_ctx, PROVIDER, "Mock stream error: Failed to start stream");
    }

    return OK(NULL);
}

// Mock control functions
void ik_test_mock_set_provider_failure(bool should_fail)
{
    mock_get_provider_should_fail = should_fail;
}

void ik_test_mock_set_request_failure(bool should_fail)
{
    mock_build_request_should_fail = should_fail;
}

void ik_test_mock_set_stream_failure(bool should_fail)
{
    mock_start_stream_should_fail = should_fail;
}

void ik_test_mock_reset_flags(void)
{
    mock_get_provider_should_fail = false;
    mock_build_request_should_fail = false;
    mock_start_stream_should_fail = false;
}