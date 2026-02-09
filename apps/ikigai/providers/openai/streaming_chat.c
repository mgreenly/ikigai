/**
 * @file streaming_chat.c
 * @brief OpenAI Chat Completions streaming implementation
 */

#include "apps/ikigai/providers/openai/streaming_chat_internal.h"
#include "apps/ikigai/providers/openai/error.h"
#include "shared/panic.h"
#include "vendor/yyjson/yyjson.h"
#include <assert.h>
#include <string.h>


#include "shared/poison.h"
/* ================================================================
 * Context Creation
 * ================================================================ */

ik_openai_chat_stream_ctx_t *ik_openai_chat_stream_ctx_create(TALLOC_CTX *ctx,
                                                              ik_stream_cb_t stream_cb,
                                                              void *stream_ctx)
{
    assert(ctx != NULL);        // LCOV_EXCL_BR_LINE
    assert(stream_cb != NULL);  // LCOV_EXCL_BR_LINE

    // Allocate streaming context
    ik_openai_chat_stream_ctx_t *sctx = talloc_zero(ctx, ik_openai_chat_stream_ctx_t);
    if (sctx == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Store user callbacks
    sctx->stream_cb = stream_cb;
    sctx->stream_ctx = stream_ctx;

    // Initialize state
    sctx->model = NULL;
    sctx->finish_reason = IK_FINISH_UNKNOWN;
    memset(&sctx->usage, 0, sizeof(ik_usage_t));
    sctx->started = false;
    sctx->in_tool_call = false;
    sctx->tool_call_index = -1;
    sctx->current_tool_id = NULL;
    sctx->current_tool_name = NULL;
    sctx->current_tool_args = NULL;

    return sctx;
}

/* ================================================================
 * Response Builder
 * ================================================================ */

ik_response_t *ik_openai_chat_stream_build_response(TALLOC_CTX *ctx,
                                                    ik_openai_chat_stream_ctx_t *sctx)
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
        // Allocate content blocks array with one tool call (zero-init to avoid garbage pointers)
        resp->content_blocks = talloc_zero_array(resp, ik_content_block_t, 1);
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
        // thought_signature is NULL (zero-initialized) - Chat API doesn't provide it
    } else {
        // No tool call - empty content
        resp->content_blocks = NULL;
        resp->content_count = 0;
    }

    return resp;
}

/* ================================================================
 * Getters
 * ================================================================ */

ik_usage_t ik_openai_chat_stream_get_usage(ik_openai_chat_stream_ctx_t *stream_ctx)
{
    assert(stream_ctx != NULL); // LCOV_EXCL_BR_LINE
    return stream_ctx->usage;
}

/* ================================================================
 * Data Processing Helpers
 * ================================================================ */

static void process_done_marker(ik_openai_chat_stream_ctx_t *stream_ctx)
{
    ik_openai_chat_maybe_end_tool_call(stream_ctx);

    ik_stream_event_t event = {
        .type = IK_STREAM_DONE,
        .index = 0,
        .data.done.finish_reason = stream_ctx->finish_reason,
        .data.done.usage = stream_ctx->usage,
        .data.done.provider_data = NULL
    };
    stream_ctx->stream_cb(&event, stream_ctx->stream_ctx);
}

static ik_error_category_t map_error_type(const char *type)
{
    if (type == NULL) {
        return IK_ERR_CAT_UNKNOWN;
    }

    if (strstr(type, "authentication") != NULL || strstr(type, "permission") != NULL) {
        return IK_ERR_CAT_AUTH;
    }
    if (strstr(type, "rate_limit") != NULL) {
        return IK_ERR_CAT_RATE_LIMIT;
    }
    if (strstr(type, "invalid_request") != NULL) {
        return IK_ERR_CAT_INVALID_ARG;
    }
    if (strstr(type, "server") != NULL || strstr(type, "service") != NULL) {
        return IK_ERR_CAT_SERVER;
    }

    return IK_ERR_CAT_UNKNOWN;
}

static bool process_error_object(ik_openai_chat_stream_ctx_t *stream_ctx, yyjson_val *error_val)
{
    if (error_val == NULL || !yyjson_is_obj(error_val)) {
        return false;
    }

    yyjson_val *message_val = yyjson_obj_get(error_val, "message"); // LCOV_EXCL_BR_LINE
    yyjson_val *type_val = yyjson_obj_get(error_val, "type");

    const char *message = yyjson_get_str(message_val);
    const char *type = yyjson_get_str(type_val);

    ik_error_category_t category = map_error_type(type);

    ik_stream_event_t event = {
        .type = IK_STREAM_ERROR,
        .index = 0,
        .data.error.category = category,
        .data.error.message = message ? message : "Unknown error"
    };
    stream_ctx->stream_cb(&event, stream_ctx->stream_ctx);
    return true;
}

static void extract_model_if_needed(ik_openai_chat_stream_ctx_t *stream_ctx, yyjson_val *root)
{
    if (stream_ctx->model != NULL) {
        return;
    }

    yyjson_val *model_val = yyjson_obj_get(root, "model");
    if (model_val == NULL) {
        return;
    }

    const char *model = yyjson_get_str(model_val);
    if (model == NULL) {
        return;
    }

    stream_ctx->model = talloc_strdup(stream_ctx, model);
    if (stream_ctx->model == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
}

static void process_choices_array(ik_openai_chat_stream_ctx_t *stream_ctx, yyjson_val *choices_val)
{
    if (choices_val == NULL || !yyjson_is_arr(choices_val)) {
        return;
    }

    size_t choices_size = yyjson_arr_size(choices_val);
    if (choices_size == 0) {
        return;
    }

    yyjson_val *choice0 = yyjson_arr_get(choices_val, 0);
    if (choice0 == NULL || !yyjson_is_obj(choice0)) { // LCOV_EXCL_BR_LINE
        return;
    }

    yyjson_val *delta_val = yyjson_obj_get(choice0, "delta");
    if (delta_val == NULL || !yyjson_is_obj(delta_val)) {
        return;
    }

    yyjson_val *finish_reason_val = yyjson_obj_get(choice0, "finish_reason");
    const char *finish_reason_str = NULL;
    if (finish_reason_val != NULL && yyjson_is_str(finish_reason_val)) {
        finish_reason_str = yyjson_get_str(finish_reason_val);
    }

    ik_openai_chat_process_delta(stream_ctx, delta_val, finish_reason_str);
}

static void extract_usage_statistics(ik_openai_chat_stream_ctx_t *stream_ctx, yyjson_val *usage_val)
{
    if (usage_val == NULL || !yyjson_is_obj(usage_val)) {
        return;
    }

    yyjson_val *prompt_tokens_val = yyjson_obj_get(usage_val, "prompt_tokens");
    if (prompt_tokens_val != NULL && yyjson_is_int(prompt_tokens_val)) {
        stream_ctx->usage.input_tokens = (int32_t)yyjson_get_int(prompt_tokens_val);
    }

    yyjson_val *completion_tokens_val = yyjson_obj_get(usage_val, "completion_tokens");
    if (completion_tokens_val != NULL && yyjson_is_int(completion_tokens_val)) {
        stream_ctx->usage.output_tokens = (int32_t)yyjson_get_int(completion_tokens_val);
    }

    yyjson_val *total_tokens_val = yyjson_obj_get(usage_val, "total_tokens");
    if (total_tokens_val != NULL && yyjson_is_int(total_tokens_val)) {
        stream_ctx->usage.total_tokens = (int32_t)yyjson_get_int(total_tokens_val);
    }

    yyjson_val *details_val = yyjson_obj_get(usage_val, "completion_tokens_details");
    if (details_val == NULL || !yyjson_is_obj(details_val)) {
        return;
    }

    yyjson_val *reasoning_tokens_val = yyjson_obj_get(details_val, "reasoning_tokens");
    if (reasoning_tokens_val != NULL && yyjson_is_int(reasoning_tokens_val)) {
        stream_ctx->usage.thinking_tokens = (int32_t)yyjson_get_int(reasoning_tokens_val);
    }
}

/* ================================================================
 * Data Processing
 * ================================================================ */

void ik_openai_chat_stream_process_data(ik_openai_chat_stream_ctx_t *stream_ctx, const char *data)
{
    assert(stream_ctx != NULL); // LCOV_EXCL_BR_LINE
    assert(data != NULL); // LCOV_EXCL_BR_LINE

    if (strcmp(data, "[DONE]") == 0) {
        process_done_marker(stream_ctx);
        return;
    }

    yyjson_doc *doc = yyjson_read(data, strlen(data), 0);
    if (doc == NULL) {
        return;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (root == NULL || !yyjson_is_obj(root)) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return;
    }

    yyjson_val *error_val = yyjson_obj_get(root, "error");
    if (process_error_object(stream_ctx, error_val)) {
        yyjson_doc_free(doc);
        return;
    }

    extract_model_if_needed(stream_ctx, root);

    yyjson_val *choices_val = yyjson_obj_get(root, "choices");
    process_choices_array(stream_ctx, choices_val);

    yyjson_val *usage_val = yyjson_obj_get(root, "usage");
    extract_usage_statistics(stream_ctx, usage_val);

    yyjson_doc_free(doc);
}
