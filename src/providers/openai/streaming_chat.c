/**
 * @file streaming_chat.c
 * @brief OpenAI Chat Completions streaming implementation
 */

#include "streaming_chat_internal.h"
#include "error.h"
#include "panic.h"
#include "vendor/yyjson/yyjson.h"
#include <assert.h>
#include <string.h>

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
 * Getters
 * ================================================================ */

ik_usage_t ik_openai_chat_stream_get_usage(ik_openai_chat_stream_ctx_t *stream_ctx)
{
    assert(stream_ctx != NULL); // LCOV_EXCL_BR_LINE
    return stream_ctx->usage;
}

ik_finish_reason_t ik_openai_chat_stream_get_finish_reason(ik_openai_chat_stream_ctx_t *stream_ctx)
{
    assert(stream_ctx != NULL); // LCOV_EXCL_BR_LINE
    return stream_ctx->finish_reason;
}

/* ================================================================
 * Data Processing
 * ================================================================ */

void ik_openai_chat_stream_process_data(ik_openai_chat_stream_ctx_t *stream_ctx, const char *data)
{
    assert(stream_ctx != NULL); // LCOV_EXCL_BR_LINE
    assert(data != NULL); // LCOV_EXCL_BR_LINE

    // Check for [DONE] marker
    if (strcmp(data, "[DONE]") == 0) {
        // End any active tool call
        ik_openai_chat_maybe_end_tool_call(stream_ctx);

        // Emit DONE event
        ik_stream_event_t event = {
            .type = IK_STREAM_DONE,
            .index = 0,
            .data.done.finish_reason = stream_ctx->finish_reason,
            .data.done.usage = stream_ctx->usage,
            .data.done.provider_data = NULL
        };
        stream_ctx->stream_cb(&event, stream_ctx->stream_ctx);
        return;
    }

    // Parse JSON
    yyjson_doc *doc = yyjson_read(data, strlen(data), 0);
    if (doc == NULL) {
        // Malformed JSON - skip silently
        return;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (root == NULL || !yyjson_is_obj(root)) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return;
    }

    // Check for error
    yyjson_val *error_val = yyjson_obj_get(root, "error");
    if (error_val != NULL && yyjson_is_obj(error_val)) {
        // Extract error message and type
        yyjson_val *message_val = yyjson_obj_get(error_val, "message"); // LCOV_EXCL_BR_LINE
        yyjson_val *type_val = yyjson_obj_get(error_val, "type");

        const char *message = yyjson_get_str(message_val);
        const char *type = yyjson_get_str(type_val);

        // Map error type to category
        ik_error_category_t category = IK_ERR_CAT_UNKNOWN;
        if (type != NULL) {
            if (strstr(type, "authentication") != NULL || strstr(type, "permission") != NULL) {
                category = IK_ERR_CAT_AUTH;
            } else if (strstr(type, "rate_limit") != NULL) {
                category = IK_ERR_CAT_RATE_LIMIT;
            } else if (strstr(type, "invalid_request") != NULL) {
                category = IK_ERR_CAT_INVALID_ARG;
            } else if (strstr(type, "server") != NULL || strstr(type, "service") != NULL) {
                category = IK_ERR_CAT_SERVER;
            }
        }

        // Emit ERROR event
        ik_stream_event_t event = {
            .type = IK_STREAM_ERROR,
            .index = 0,
            .data.error.category = category,
            .data.error.message = message ? message : "Unknown error"
        };
        stream_ctx->stream_cb(&event, stream_ctx->stream_ctx);
        yyjson_doc_free(doc);
        return;
    }

    // Extract model from first chunk
    if (stream_ctx->model == NULL) {
        yyjson_val *model_val = yyjson_obj_get(root, "model");
        if (model_val != NULL) {
            const char *model = yyjson_get_str(model_val);
            if (model != NULL) {
                stream_ctx->model = talloc_strdup(stream_ctx, model);
                if (stream_ctx->model == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            }
        }
    }

    // Extract choices array
    yyjson_val *choices_val = yyjson_obj_get(root, "choices");
    if (choices_val != NULL && yyjson_is_arr(choices_val)) {
        size_t choices_size = yyjson_arr_size(choices_val);
        if (choices_size > 0) {
            yyjson_val *choice0 = yyjson_arr_get(choices_val, 0);
            if (choice0 != NULL && yyjson_is_obj(choice0)) { // LCOV_EXCL_BR_LINE
                // Extract delta
                yyjson_val *delta_val = yyjson_obj_get(choice0, "delta");
                if (delta_val != NULL && yyjson_is_obj(delta_val)) {
                    // Extract finish_reason
                    yyjson_val *finish_reason_val = yyjson_obj_get(choice0, "finish_reason");
                    const char *finish_reason_str = NULL;
                    if (finish_reason_val != NULL && yyjson_is_str(finish_reason_val)) {
                        finish_reason_str = yyjson_get_str(finish_reason_val);
                    }

                    // Process delta
                    ik_openai_chat_process_delta(stream_ctx, delta_val, finish_reason_str);
                }
            }
        }
    }

    // Extract usage (final chunk with stream_options.include_usage)
    yyjson_val *usage_val = yyjson_obj_get(root, "usage");
    if (usage_val != NULL && yyjson_is_obj(usage_val)) {
        // Extract prompt_tokens
        yyjson_val *prompt_tokens_val = yyjson_obj_get(usage_val, "prompt_tokens");
        if (prompt_tokens_val != NULL && yyjson_is_int(prompt_tokens_val)) {
            stream_ctx->usage.input_tokens = (int32_t)yyjson_get_int(prompt_tokens_val);
        }

        // Extract completion_tokens
        yyjson_val *completion_tokens_val = yyjson_obj_get(usage_val, "completion_tokens");
        if (completion_tokens_val != NULL && yyjson_is_int(completion_tokens_val)) {
            stream_ctx->usage.output_tokens = (int32_t)yyjson_get_int(completion_tokens_val);
        }

        // Extract total_tokens
        yyjson_val *total_tokens_val = yyjson_obj_get(usage_val, "total_tokens");
        if (total_tokens_val != NULL && yyjson_is_int(total_tokens_val)) {
            stream_ctx->usage.total_tokens = (int32_t)yyjson_get_int(total_tokens_val);
        }

        // Extract completion_tokens_details.reasoning_tokens (thinking tokens)
        yyjson_val *details_val = yyjson_obj_get(usage_val, "completion_tokens_details");
        if (details_val != NULL && yyjson_is_obj(details_val)) {
            yyjson_val *reasoning_tokens_val = yyjson_obj_get(details_val, "reasoning_tokens");
            if (reasoning_tokens_val != NULL && yyjson_is_int(reasoning_tokens_val)) {
                stream_ctx->usage.thinking_tokens = (int32_t)yyjson_get_int(reasoning_tokens_val);
            }
        }
    }

    yyjson_doc_free(doc);
}
