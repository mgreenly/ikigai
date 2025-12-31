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

res_t ik_anthropic_parse_response(TALLOC_CTX *ctx, const char *json, size_t json_len,
                                   ik_response_t **out_resp)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(json != NULL);     // LCOV_EXCL_BR_LINE
    assert(out_resp != NULL); // LCOV_EXCL_BR_LINE

    // Parse JSON with talloc allocator
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    // yyjson_read_opts wants non-const pointer but doesn't modify the data (same cast pattern as yyjson.h:993)
    yyjson_doc *doc = yyjson_read_opts((char *)(void *)(size_t)(const void *)json, json_len, 0, &allocator, NULL);
    if (doc == NULL) {
        return ERR(ctx, PARSE, "Invalid JSON response");
    }

    yyjson_val *root = yyjson_doc_get_root(doc); // LCOV_EXCL_BR_LINE
    if (!yyjson_is_obj(root)) {
        return ERR(ctx, PARSE, "Response root is not an object");
    }

    // Check for error response
    yyjson_val *type_val = yyjson_obj_get(root, "type");
    if (type_val != NULL) {
        const char *type_str = yyjson_get_str(type_val);
        if (type_str != NULL && strcmp(type_str, "error") == 0) {
            // Extract error message
            yyjson_val *error_obj = yyjson_obj_get(root, "error");
            const char *error_msg = "Unknown error";
            if (error_obj != NULL) {
                yyjson_val *msg_val = yyjson_obj_get(error_obj, "message");
                if (msg_val != NULL) {
                    const char *msg = yyjson_get_str(msg_val);
                    if (msg != NULL) {
                        error_msg = msg;
                    }
                }
            }
            return ERR(ctx, PROVIDER, "API error: %s", error_msg);
        }
    }

    // Allocate response structure
    ik_response_t *resp = talloc_zero(ctx, ik_response_t);
    if (resp == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Extract model
    yyjson_val *model_val = yyjson_obj_get(root, "model");
    if (model_val != NULL) {
        const char *model = yyjson_get_str(model_val);
        if (model != NULL) {
            resp->model = talloc_strdup(resp, model);
            if (resp->model == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        }
    }

    // Extract stop_reason and map to finish_reason
    yyjson_val *stop_reason_val = yyjson_obj_get(root, "stop_reason");
    const char *stop_reason = NULL;
    if (stop_reason_val != NULL) {
        stop_reason = yyjson_get_str(stop_reason_val);
    }
    resp->finish_reason = ik_anthropic_map_finish_reason(stop_reason); // LCOV_EXCL_BR_LINE

    // Extract usage
    yyjson_val *usage_obj = yyjson_obj_get(root, "usage");
    ik_anthropic_parse_usage(usage_obj, &resp->usage);

    // Extract content blocks
    yyjson_val *content_arr = yyjson_obj_get(root, "content");
    if (content_arr != NULL && yyjson_is_arr(content_arr)) {
        res_t result = ik_anthropic_parse_content_blocks(resp, content_arr, &resp->content_blocks,
                                                          &resp->content_count);
        if (is_err(&result)) {
            return result;
        }
    } else {
        resp->content_blocks = NULL;
        resp->content_count = 0;
    }

    *out_resp = resp;
    return OK(resp);
}

res_t ik_anthropic_parse_error(TALLOC_CTX *ctx, int http_status, const char *json,
                                size_t json_len, ik_error_category_t *out_category,
                                char **out_message)
{
    assert(ctx != NULL);         // LCOV_EXCL_BR_LINE
    assert(out_category != NULL); // LCOV_EXCL_BR_LINE
    assert(out_message != NULL);  // LCOV_EXCL_BR_LINE

    // Map HTTP status to category
    switch (http_status) {
        case 400:
            *out_category = IK_ERR_CAT_INVALID_ARG;
            break;
        case 401:
        case 403:
            *out_category = IK_ERR_CAT_AUTH;
            break;
        case 404:
            *out_category = IK_ERR_CAT_NOT_FOUND;
            break;
        case 429:
            *out_category = IK_ERR_CAT_RATE_LIMIT;
            break;
        case 500:
        case 502:
        case 503:
        case 529:
            *out_category = IK_ERR_CAT_SERVER;
            break;
        default:
            *out_category = IK_ERR_CAT_UNKNOWN;
            break;
    }

    // Try to extract error message from JSON
    if (json != NULL && json_len > 0) {
        yyjson_alc allocator = ik_make_talloc_allocator(ctx);
        // yyjson_read_opts wants non-const pointer but doesn't modify the data (same cast pattern as yyjson.h:993)
        yyjson_doc *doc = yyjson_read_opts((char *)(void *)(size_t)(const void *)json, json_len, 0, &allocator, NULL);
        if (doc != NULL) {
            yyjson_val *root = yyjson_doc_get_root(doc); // LCOV_EXCL_BR_LINE
            if (yyjson_is_obj(root)) {
                yyjson_val *error_obj = yyjson_obj_get(root, "error");
                if (error_obj != NULL) {
                    yyjson_val *type_val = yyjson_obj_get(error_obj, "type"); // LCOV_EXCL_BR_LINE
                    yyjson_val *msg_val = yyjson_obj_get(error_obj, "message");

                    const char *type_str = NULL;
                    const char *msg_str = NULL;

                    if (type_val != NULL) {
                        type_str = yyjson_get_str(type_val);
                    }
                    if (msg_val != NULL) {
                        msg_str = yyjson_get_str(msg_val);
                    }

                    if (type_str != NULL && msg_str != NULL) {
                        *out_message = talloc_asprintf(ctx, "%s: %s", type_str, msg_str);
                    } else if (msg_str != NULL) {
                        *out_message = talloc_strdup(ctx, msg_str);
                    } else if (type_str != NULL) {
                        *out_message = talloc_strdup(ctx, type_str);
                    } else {
                        *out_message = talloc_asprintf(ctx, "HTTP %d", http_status);
                    }

                    if (*out_message == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
                    return OK(NULL);
                }
            }
        }
    }

    // Fallback to HTTP status message
    *out_message = talloc_asprintf(ctx, "HTTP %d", http_status);
    if (*out_message == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    return OK(NULL);
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
