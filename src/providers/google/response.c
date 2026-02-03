/**
 * @file response.c
 * @brief Google response parsing implementation
 */

#include "response.h"
#include "response_utils.h"
#include "json_allocator.h"
#include "panic.h"
#include "vendor/yyjson/yyjson.h"
#include <assert.h>
#include <string.h>


#include "poison.h"


/* ================================================================
 * Vtable Implementations (Stubs for Future Tasks)
 * ================================================================ */

res_t ik_google_start_request(void *impl_ctx, const ik_request_t *req,
                              ik_provider_completion_cb_t cb, void *cb_ctx)
{
    assert(impl_ctx != NULL); // LCOV_EXCL_BR_LINE
    assert(req != NULL);      // LCOV_EXCL_BR_LINE
    assert(cb != NULL);       // LCOV_EXCL_BR_LINE

    (void)impl_ctx;
    (void)req;
    (void)cb;
    (void)cb_ctx;

    // Stub: Will be implemented in google-http.md task
    return OK(NULL);
}

