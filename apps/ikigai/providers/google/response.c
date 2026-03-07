/**
 * @file response.c
 * @brief Google response parsing implementation
 */

#include "apps/ikigai/providers/google/response.h"
#include "apps/ikigai/providers/google/google_internal.h"
#include "apps/ikigai/providers/google/request.h"
#include "apps/ikigai/providers/google/response_utils.h"
#include "apps/ikigai/providers/common/http_multi.h"
#include "shared/json_allocator.h"
#include "shared/panic.h"
#include "vendor/yyjson/yyjson.h"
#include <assert.h>
#include <string.h>

#include "shared/poison.h"

/* ================================================================
 * Non-Streaming Request Implementation
 * ================================================================ */

/* Context for tracking a non-streaming HTTP request */
typedef struct {
    ik_google_ctx_t *impl_ctx;
    ik_provider_completion_cb_t cb;
    void *cb_ctx;
} google_request_ctx_t;

/* Parse content parts from candidate into response content blocks */
static void parse_content_parts(ik_response_t *resp, yyjson_val *parts)
{
    size_t count = yyjson_arr_size(parts);
    resp->content_blocks = talloc_zero_array(resp, ik_content_block_t, (unsigned int)count);
    if (!resp->content_blocks) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    resp->content_count = 0;

    size_t idx, max;
    yyjson_val *part;
    yyjson_arr_foreach(parts, idx, max, part) {
        (void)idx;
        yyjson_val *text_val = yyjson_obj_get(part, "text");
        if (!yyjson_is_str(text_val)) {
            continue;
        }
        ik_content_block_t *cb = &resp->content_blocks[resp->content_count];
        cb->type = IK_CONTENT_TEXT;
        cb->data.text.text = talloc_strdup(resp, yyjson_get_str(text_val));
        resp->content_count++;
    }
}

/* Parse candidate fields into response */
static void parse_candidate(ik_response_t *resp, yyjson_val *candidate)
{
    yyjson_val *finish_val = yyjson_obj_get(candidate, "finishReason");
    resp->finish_reason = yyjson_is_str(finish_val)
        ? ik_google_map_finish_reason(yyjson_get_str(finish_val))
        : IK_FINISH_UNKNOWN;

    yyjson_val *content = yyjson_obj_get(candidate, "content");
    if (!yyjson_is_obj(content)) {
        return;
    }
    yyjson_val *parts = yyjson_obj_get(content, "parts");
    if (yyjson_is_arr(parts)) {
        parse_content_parts(resp, parts);
    }
}

/* Parse usageMetadata into response usage */
static void parse_usage(ik_response_t *resp, yyjson_val *usage_obj)
{
    yyjson_val *prompt_tokens = yyjson_obj_get(usage_obj, "promptTokenCount");
    yyjson_val *candidates_tokens = yyjson_obj_get(usage_obj, "candidatesTokenCount");
    yyjson_val *thoughts_tokens = yyjson_obj_get(usage_obj, "thoughtsTokenCount");
    yyjson_val *total_tokens = yyjson_obj_get(usage_obj, "totalTokenCount");

    int32_t prompt = prompt_tokens ? (int32_t)yyjson_get_int(prompt_tokens) : 0;
    int32_t output = candidates_tokens ? (int32_t)yyjson_get_int(candidates_tokens) : 0;
    int32_t thoughts = thoughts_tokens ? (int32_t)yyjson_get_int(thoughts_tokens) : 0;

    resp->usage.input_tokens = prompt;
    resp->usage.thinking_tokens = thoughts;
    resp->usage.output_tokens = output - thoughts;
    resp->usage.total_tokens = total_tokens ? (int32_t)yyjson_get_int(total_tokens) : 0;
    resp->usage.cached_tokens = 0;
}

/* Parse Google non-streaming JSON response into ik_response_t */
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

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return NULL;
    }

    ik_response_t *resp = talloc_zero(ctx, ik_response_t);
    if (!resp) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    yyjson_val *model_val = yyjson_obj_get(root, "modelVersion");
    if (yyjson_is_str(model_val)) {
        resp->model = talloc_strdup(resp, yyjson_get_str(model_val));
    }

    yyjson_val *candidates = yyjson_obj_get(root, "candidates");
    if (yyjson_is_arr(candidates)) {
        yyjson_val *candidate = yyjson_arr_get_first(candidates);
        if (candidate != NULL) {
            parse_candidate(resp, candidate);
        }
    }

    yyjson_val *usage_obj = yyjson_obj_get(root, "usageMetadata");
    if (yyjson_is_obj(usage_obj)) {
        parse_usage(resp, usage_obj);
    }

    yyjson_doc_free(doc);
    return resp;
}

/* HTTP completion callback for non-streaming Google requests */
static void google_http_completion_cb(const ik_http_completion_t *http, void *user_ctx)
{
    google_request_ctx_t *req_ctx = (google_request_ctx_t *)user_ctx;
    assert(req_ctx != NULL);     // LCOV_EXCL_BR_LINE
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

        ik_google_parse_error(req_ctx, http->http_code,
                              http->response_body, http->response_len,
                              &completion.error_category, &completion.error_message);
    }

    req_ctx->cb(&completion, req_ctx->cb_ctx);
    talloc_free(req_ctx);
}

res_t ik_google_start_request(void *impl_ctx_void, const ik_request_t *req,
                              ik_provider_completion_cb_t cb, void *cb_ctx)
{
    assert(impl_ctx_void != NULL); // LCOV_EXCL_BR_LINE
    assert(req != NULL);           // LCOV_EXCL_BR_LINE
    assert(cb != NULL);            // LCOV_EXCL_BR_LINE

    ik_google_ctx_t *impl_ctx = (ik_google_ctx_t *)impl_ctx_void;

    /* Create request context */
    google_request_ctx_t *req_ctx = talloc_zero(impl_ctx, google_request_ctx_t);
    if (!req_ctx) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    req_ctx->impl_ctx = impl_ctx;
    req_ctx->cb = cb;
    req_ctx->cb_ctx = cb_ctx;

    /* Build URL (non-streaming) */
    char *url = NULL;
    res_t r = ik_google_build_url(req_ctx, impl_ctx->base_url, req->model,
                                  impl_ctx->api_key, false, &url);
    if (is_err(&r)) {
        talloc_steal(impl_ctx, r.err);
        talloc_free(req_ctx);
        return r;
    }

    /* Serialize request JSON */
    char *body = NULL;
    r = ik_google_serialize_request(req_ctx, req, &body);
    if (is_err(&r)) {
        talloc_steal(impl_ctx, r.err);
        talloc_free(req_ctx);
        return r;
    }

    /* Build headers (non-streaming) */
    char **headers = NULL;
    r = ik_google_build_headers(req_ctx, false, &headers);
    if (is_err(&r)) {
        talloc_steal(impl_ctx, r.err);
        talloc_free(req_ctx);
        return r;
    }

    ik_http_request_t http_req = {
        .url = url,
        .method = "POST",
        .headers = (const char **)(void *)headers,
        .body = body,
        .body_len = strlen(body),
    };

    r = ik_http_multi_add_request(impl_ctx->http_multi, &http_req,
                                  NULL, NULL,
                                  google_http_completion_cb, req_ctx);
    if (is_err(&r)) {
        talloc_steal(impl_ctx, r.err);
        talloc_free(req_ctx);
        return r;
    }

    return OK(NULL);
}
