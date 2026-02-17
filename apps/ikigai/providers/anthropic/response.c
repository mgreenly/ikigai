/**
 * @file response.c
 * @brief Anthropic response parsing implementation
 */

#include "apps/ikigai/providers/anthropic/response.h"

#include "shared/json_allocator.h"
#include "shared/panic.h"
#include "apps/ikigai/providers/anthropic/request.h"
#include "apps/ikigai/providers/anthropic/response_helpers.h"
#include "apps/ikigai/providers/anthropic/streaming.h"
#include "vendor/yyjson/yyjson.h"

#include <assert.h>
#include <string.h>


#include "shared/poison.h"
/* ================================================================
 * Public Functions
 * ================================================================ */

ik_finish_reason_t ik_anthropic_map_finish_reason(const char *stop_reason)
{
    if (stop_reason == NULL) {
        return IK_FINISH_UNKNOWN;
    }

    if (strcmp(stop_reason, "end_turn") == 0) {
        return IK_FINISH_STOP;
    } else if (strcmp(stop_reason, "max_tokens") == 0) {
        return IK_FINISH_LENGTH;
    } else if (strcmp(stop_reason, "tool_use") == 0) {
        return IK_FINISH_TOOL_USE;
    } else if (strcmp(stop_reason, "stop_sequence") == 0) {
        return IK_FINISH_STOP;
    } else if (strcmp(stop_reason, "refusal") == 0) {
        return IK_FINISH_CONTENT_FILTER;
    }

    return IK_FINISH_UNKNOWN;
}

/* ================================================================
 * Async Vtable Implementations (Stubs)
 * ================================================================ */

res_t ik_anthropic_start_request(void *impl_ctx, const ik_request_t *req,
                                 ik_provider_completion_cb_t cb, void *cb_ctx)
{
    assert(impl_ctx != NULL); // LCOV_EXCL_BR_LINE
    assert(req != NULL);      // LCOV_EXCL_BR_LINE
    assert(cb != NULL);       // LCOV_EXCL_BR_LINE

    (void)impl_ctx;
    (void)req;
    (void)cb;
    (void)cb_ctx;

    // Stub: Will be implemented when HTTP multi layer is ready
    return OK(NULL);
}
