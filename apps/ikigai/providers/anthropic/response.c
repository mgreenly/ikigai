/**
 * @file response.c
 * @brief Anthropic response parsing implementation
 */

#include "apps/ikigai/providers/anthropic/response.h"

#include "shared/json_allocator.h"
#include "shared/panic.h"
#include "apps/ikigai/providers/anthropic/anthropic_internal.h"
#include "apps/ikigai/providers/anthropic/request.h"
#include "apps/ikigai/providers/anthropic/response_helpers.h"
#include "apps/ikigai/providers/anthropic/streaming.h"
#include "apps/ikigai/providers/common/http_multi.h"
#include "shared/wrapper_internal.h"
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
 * Non-Streaming Request Implementation
 * ================================================================ */

/* Context for tracking a non-streaming HTTP request */
typedef struct {
    ik_provider_completion_cb_t cb;
    void *cb_ctx;
} anthropic_request_ctx_t;

/* Parse Anthropic non-streaming JSON response into ik_response_t */
static ik_response_t *parse_response(TALLOC_CTX *ctx,
                                     const char *body,
                                     size_t body_len)
{
    if (body == NULL || body_len == 0) {
        return NULL;
    }

    yyjson_doc *doc = yyjson_read(body, body_len, 0);
    if (!doc) {
        return NULL;
    }

    yyjson_val *root = yyjson_doc_get_root(doc); // LCOV_EXCL_BR_LINE
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return NULL;
    }

    ik_response_t *resp = talloc_zero(ctx, ik_response_t);
    if (!resp) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    /* model */
    yyjson_val *model = yyjson_obj_get(root, "model");
    if (yyjson_is_str(model)) {
        resp->model = talloc_strdup(resp, yyjson_get_str(model));
    }

    /* stop_reason */
    yyjson_val *stop_reason = yyjson_obj_get(root, "stop_reason");
    if (yyjson_is_str(stop_reason)) {
        resp->finish_reason = ik_anthropic_map_finish_reason(yyjson_get_str(stop_reason));
    } else {
        resp->finish_reason = IK_FINISH_UNKNOWN;
    }

    /* usage */
    yyjson_val *usage = yyjson_obj_get(root, "usage"); // LCOV_EXCL_BR_LINE
    if (yyjson_is_obj(usage)) {
        yyjson_val *input_tokens = yyjson_obj_get(usage, "input_tokens"); // LCOV_EXCL_BR_LINE
        yyjson_val *output_tokens = yyjson_obj_get(usage, "output_tokens"); // LCOV_EXCL_BR_LINE
        if (yyjson_is_int(input_tokens)) {
            resp->usage.input_tokens = (int32_t)yyjson_get_int(input_tokens);
        }
        if (yyjson_is_int(output_tokens)) {
            resp->usage.output_tokens = (int32_t)yyjson_get_int(output_tokens);
        }
        resp->usage.total_tokens = resp->usage.input_tokens + resp->usage.output_tokens;
    }

    /* content */
    yyjson_val *content = yyjson_obj_get(root, "content");
    if (yyjson_is_arr(content)) {
        size_t count = yyjson_arr_size(content);
        resp->content_blocks = talloc_zero_array(resp, ik_content_block_t, (unsigned int)count);
        if (!resp->content_blocks) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        resp->content_count = 0;

        size_t idx, max;
        yyjson_val *block;
        yyjson_arr_foreach(content, idx, max, block) { // LCOV_EXCL_BR_LINE
            (void)idx;
            yyjson_val *type = yyjson_obj_get(block, "type");
            if (!yyjson_is_str(type)) {
                continue;
            }
            const char *type_str = yyjson_get_str(type);
            ik_content_block_t *cb = &resp->content_blocks[resp->content_count];

            if (strcmp(type_str, "text") == 0) {
                cb->type = IK_CONTENT_TEXT; // LCOV_EXCL_BR_LINE
                yyjson_val *text = yyjson_obj_get(block, "text"); // LCOV_EXCL_BR_LINE
                cb->data.text.text = yyjson_is_str(text)
                    ? talloc_strdup(resp, yyjson_get_str(text))
                    : talloc_strdup(resp, "");
                resp->content_count++;
            }
        }
    }

    yyjson_doc_free(doc);
    return resp;
}

/* HTTP completion callback for non-streaming Anthropic requests */
static void anthropic_http_completion_cb(const ik_http_completion_t *http, void *user_ctx)
{
    anthropic_request_ctx_t *req_ctx = (anthropic_request_ctx_t *)user_ctx;
    assert(req_ctx != NULL);    // LCOV_EXCL_BR_LINE
    assert(req_ctx->cb != NULL); // LCOV_EXCL_BR_LINE

    ik_provider_completion_t completion = {0};
    completion.http_status = http->http_code;
    completion.retry_after_ms = -1;

    if (http->type == IK_HTTP_SUCCESS) {
        ik_response_t *resp = parse_response(req_ctx, http->response_body, http->response_len);
        if (resp) {
            completion.success = true;
            completion.response = resp;
            completion.error_category = IK_ERR_CAT_UNKNOWN;
            completion.error_message = NULL;
        } else {
            completion.success = false;
            completion.response = NULL;
            completion.error_category = IK_ERR_CAT_UNKNOWN;
            completion.error_message = talloc_strdup(req_ctx, "Failed to parse response body");
        }
    } else {
        completion.success = false;
        completion.response = NULL;

        if (http->http_code == 401 || http->http_code == 403) {
            completion.error_category = IK_ERR_CAT_AUTH;
        } else if (http->http_code == 429) {
            completion.error_category = IK_ERR_CAT_RATE_LIMIT;
        } else if (http->http_code >= 500) {
            completion.error_category = IK_ERR_CAT_SERVER;
        } else if (http->http_code == 0) {
            completion.error_category = IK_ERR_CAT_NETWORK;
        } else {
            completion.error_category = IK_ERR_CAT_UNKNOWN;
        }

        completion.error_message = http->error_message
            ? talloc_strdup(req_ctx, http->error_message)
            : talloc_asprintf(req_ctx, "HTTP %d", http->http_code);
    }

    req_ctx->cb(&completion, req_ctx->cb_ctx);
    talloc_free(req_ctx);
}

res_t ik_anthropic_start_request(void *impl_ctx_void, const ik_request_t *req,
                                 ik_provider_completion_cb_t cb, void *cb_ctx)
{
    assert(impl_ctx_void != NULL); // LCOV_EXCL_BR_LINE
    assert(req != NULL);           // LCOV_EXCL_BR_LINE
    assert(cb != NULL);            // LCOV_EXCL_BR_LINE

    ik_anthropic_ctx_t *impl_ctx = (ik_anthropic_ctx_t *)impl_ctx_void;

    /* Create request context */
    anthropic_request_ctx_t *req_ctx = talloc_zero(impl_ctx, anthropic_request_ctx_t);
    if (!req_ctx) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    req_ctx->cb = cb;
    req_ctx->cb_ctx = cb_ctx;

    /* Serialize request (non-streaming: includes max_tokens, omits stream:true) */
    char *body = NULL;
    res_t r = ik_anthropic_serialize_request_non_stream(req_ctx, req, &body);
    if (is_err(&r)) {
        talloc_steal(impl_ctx, r.err);
        talloc_free(req_ctx);
        return r;
    }

    /* Build URL */
    char *url = talloc_asprintf(req_ctx, "%s/v1/messages", impl_ctx->base_url);
    if (!url) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    /* Build headers */
    char *auth_header = talloc_asprintf(req_ctx, "x-api-key: %s", impl_ctx->api_key);
    if (!auth_header) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    const char *headers[] = {
        "Content-Type: application/json",
        "anthropic-version: 2023-06-01",
        auth_header,
        NULL
    };

    ik_http_request_t http_req = {
        .url = url,
        .method = "POST",
        .headers = headers,
        .body = body,
        .body_len = strlen(body),
    };

    r = ik_http_multi_add_request_(impl_ctx->http_multi, &http_req,
                                   NULL, NULL,
                                   anthropic_http_completion_cb, req_ctx);
    if (is_err(&r)) {
        talloc_steal(impl_ctx, r.err);
        talloc_free(req_ctx);
        return r;
    }

    return OK(NULL);
}
