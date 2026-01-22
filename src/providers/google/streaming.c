/**
 * @file streaming.c
 * @brief Google streaming implementation
 */

#include "streaming.h"
#include "streaming_internal.h"
#include "streaming_helpers.h"
#include "response.h"
#include "json_allocator.h"
#include "panic.h"
#include "vendor/yyjson/yyjson.h"
#include <assert.h>
#include <string.h>

/* ================================================================
 * Response Builder
 * ================================================================ */

ik_response_t *ik_google_stream_build_response(TALLOC_CTX *ctx,
                                               ik_google_stream_ctx_t *sctx)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE

    // Allocate response structure
    ik_response_t *resp = talloc_zero(ctx,
                                      ik_response_t);
    if (resp == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Copy model
    if (sctx->model != NULL) {
        resp->model = talloc_strdup(resp, sctx->model);
        if (resp->model == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    // Copy finish reason and usage
    resp->finish_reason = sctx->finish_reason;
    resp->usage = sctx->usage;

    // Check if we have a tool call to include
    if (sctx->current_tool_id != NULL && sctx->current_tool_name != NULL) {
        // Override finish_reason: Google returns "STOP" even for tool calls,
        // but we need IK_FINISH_TOOL_USE so the tool loop continues
        resp->finish_reason = IK_FINISH_TOOL_USE;

        // Allocate content blocks array with one tool call
        resp->content_blocks = talloc_array(resp, ik_content_block_t, 1);
        if (resp->content_blocks == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        resp->content_count = 1;

        // Populate tool call content block
        ik_content_block_t *block = &resp->content_blocks[0];
        block->type = IK_CONTENT_TOOL_CALL;
        block->data.tool_call.id = talloc_strdup(resp->content_blocks, sctx->current_tool_id);
        if (block->data.tool_call.id == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        block->data.tool_call.name = talloc_strdup(resp->content_blocks, sctx->current_tool_name);
        if (block->data.tool_call.name == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        block->data.tool_call.arguments = talloc_strdup(resp->content_blocks,
                                                        sctx->current_tool_args !=
                                                        NULL ? sctx->current_tool_args : "{}");
        if (block->data.tool_call.arguments == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    } else {
        // No tool call - empty content
        resp->content_blocks = NULL;
        resp->content_count = 0;
    }

    return resp;
}

/* ================================================================
 * Context Creation
 * ================================================================ */

res_t ik_google_stream_ctx_create(TALLOC_CTX *ctx, ik_stream_cb_t cb, void *cb_ctx,
                                  ik_google_stream_ctx_t **out_stream_ctx)
{
    assert(ctx != NULL);             // LCOV_EXCL_BR_LINE
    assert(cb != NULL);              // LCOV_EXCL_BR_LINE
    assert(out_stream_ctx != NULL);  // LCOV_EXCL_BR_LINE

    // Allocate streaming context
    ik_google_stream_ctx_t *sctx = talloc_zero(ctx, ik_google_stream_ctx_t);
    if (sctx == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Store user callbacks
    sctx->user_cb = cb;
    sctx->user_ctx = cb_ctx;

    // Initialize state
    sctx->model = NULL;
    sctx->finish_reason = IK_FINISH_UNKNOWN;
    memset(&sctx->usage, 0, sizeof(ik_usage_t));
    sctx->started = false;
    sctx->in_thinking = false;
    sctx->in_tool_call = false;
    sctx->current_tool_id = NULL;
    sctx->current_tool_name = NULL;
    sctx->current_tool_args = NULL;
    sctx->part_index = 0;

    *out_stream_ctx = sctx;
    return OK(sctx);
}

/* ================================================================
 * Getters
 * ================================================================ */

ik_usage_t ik_google_stream_get_usage(ik_google_stream_ctx_t *stream_ctx)
{
    assert(stream_ctx != NULL); // LCOV_EXCL_BR_LINE
    return stream_ctx->usage;
}

ik_finish_reason_t ik_google_stream_get_finish_reason(ik_google_stream_ctx_t *stream_ctx)
{
    assert(stream_ctx != NULL); // LCOV_EXCL_BR_LINE
    return stream_ctx->finish_reason;
}

/* ================================================================
 * Data Processing
 * ================================================================ */

void ik_google_stream_process_data(ik_google_stream_ctx_t *stream_ctx, const char *data)
{
    assert(stream_ctx != NULL); // LCOV_EXCL_BR_LINE

    // Skip empty data
    if (data == NULL || data[0] == '\0') {
        return;
    }

    // Parse JSON chunk
    yyjson_alc allocator = ik_make_talloc_allocator(stream_ctx);
    size_t data_len = strlen(data);
    // yyjson_read_opts wants non-const pointer but doesn't modify the data (same cast pattern as yyjson.h:993)
    yyjson_doc *doc = yyjson_read_opts((char *)(void *)(size_t)(const void *)data, data_len, 0, &allocator, NULL);
    if (doc == NULL) {
        // Silently ignore malformed JSON
        return;
    }

    yyjson_val *root = yyjson_doc_get_root(doc); // LCOV_EXCL_BR_LINE (doc NULL already checked line 360)
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return;
    }

    // Check for error object first
    yyjson_val *error_obj = yyjson_obj_get(root, "error");
    if (error_obj != NULL) {
        ik_google_stream_process_error(stream_ctx, error_obj);
        yyjson_doc_free(doc);
        return;
    }

    // Extract modelVersion on first chunk
    if (!stream_ctx->started) {
        yyjson_val *model_val = yyjson_obj_get(root, "modelVersion");
        if (model_val != NULL) {
            const char *model = yyjson_get_str(model_val);
            if (model != NULL) {
                stream_ctx->model = talloc_strdup(stream_ctx, model);
                if (stream_ctx->model == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            }
        }

        // Emit IK_STREAM_START
        ik_stream_event_t event = {
            .type = IK_STREAM_START,
            .index = 0,
            .data.start.model = stream_ctx->model
        };
        stream_ctx->user_cb(&event, stream_ctx->user_ctx);
        stream_ctx->started = true;
    }

    // Extract candidates array
    yyjson_val *candidates = yyjson_obj_get(root, "candidates");
    if (candidates != NULL && yyjson_is_arr(candidates)) {
        // Get first candidate
        yyjson_val *candidate = yyjson_arr_get_first(candidates);
        if (candidate != NULL) {
            // Extract finishReason
            yyjson_val *finish_val = yyjson_obj_get(candidate, "finishReason");
            if (finish_val != NULL) {
                const char *finish_str = yyjson_get_str(finish_val);
                stream_ctx->finish_reason = ik_google_map_finish_reason(finish_str);
            }

            // Extract content parts
            yyjson_val *content = yyjson_obj_get(candidate, "content");
            if (content != NULL) {
                yyjson_val *parts = yyjson_obj_get(content, "parts");
                if (parts != NULL && yyjson_is_arr(parts)) {
                    ik_google_stream_process_parts(stream_ctx, parts);
                }
            }
        }
    }

    // Extract usage metadata (signals end of stream)
    yyjson_val *usage_obj = yyjson_obj_get(root, "usageMetadata");
    if (usage_obj != NULL) {
        ik_google_stream_process_usage(stream_ctx, usage_obj);
    }

    yyjson_doc_free(doc);
}
