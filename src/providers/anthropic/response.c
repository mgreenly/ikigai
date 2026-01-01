/**
 * @file response.c
 * @brief Anthropic response parsing implementation
 */

#include "response.h"

#include "json_allocator.h"
#include "panic.h"
#include "request.h"
#include "response_helpers.h"
#include "streaming.h"
#include "vendor/yyjson/yyjson.h"

#include <assert.h>
#include <string.h>

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
    // TODO: Serialize request, build headers, start HTTP POST
    return OK(NULL);
}

res_t ik_anthropic_start_stream(void *impl_ctx, const ik_request_t *req,
                                 ik_stream_cb_t stream_cb, void *stream_ctx,
                                 ik_provider_completion_cb_t completion_cb,
                                 void *completion_ctx)
{
    assert(impl_ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(req != NULL);           // LCOV_EXCL_BR_LINE
    assert(stream_cb != NULL);     // LCOV_EXCL_BR_LINE
    assert(completion_cb != NULL); // LCOV_EXCL_BR_LINE

    // TODO: Implement when HTTP multi layer is ready
    // This stub satisfies the vtable requirement but doesn't perform actual streaming yet
    //
    // Implementation plan:
    // 1. Create streaming context with ik_anthropic_stream_ctx_create()
    // 2. Serialize request with ik_anthropic_serialize_request() (stream: true)
    // 3. Build headers with ik_anthropic_build_headers()
    // 4. Construct URL: base_url + "/v1/messages"
    // 5. Configure curl easy handle:
    //    - CURLOPT_URL = constructed URL
    //    - CURLOPT_POST = 1
    //    - CURLOPT_POSTFIELDS = serialized JSON
    //    - CURLOPT_HTTPHEADER = headers
    //    - CURLOPT_WRITEFUNCTION = curl_write_callback (from streaming.c)
    //    - CURLOPT_WRITEDATA = streaming context
    // 6. Add easy handle to curl_multi via ik_http_multi_add_request()
    // 7. Return OK(NULL) immediately

    (void)impl_ctx;
    (void)req;
    (void)stream_cb;
    (void)stream_ctx;
    (void)completion_cb;
    (void)completion_ctx;

    return OK(NULL);
}
