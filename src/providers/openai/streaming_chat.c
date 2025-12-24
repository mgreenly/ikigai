/**
 * @file streaming_chat.c
 * @brief OpenAI Chat Completions streaming implementation
 */

#include "streaming.h"
#include "response.h"
#include "error.h"
#include "panic.h"
#include "vendor/yyjson/yyjson.h"
#include <assert.h>
#include <string.h>

/**
 * OpenAI Chat Completions streaming context structure
 */
struct ik_openai_chat_stream_ctx {
    ik_stream_cb_t stream_cb;          /* User's stream callback */
    void *stream_ctx;                  /* User's stream context */
    char *model;                       /* Model name from first chunk */
    ik_finish_reason_t finish_reason;  /* Finish reason from choice */
    ik_usage_t usage;                  /* Accumulated usage statistics */
    bool started;                      /* Whether IK_STREAM_START was emitted */
    bool in_tool_call;                 /* Whether currently in a tool call */
    int32_t tool_call_index;           /* Current tool call index */
    char *current_tool_id;             /* Current tool call ID */
    char *current_tool_name;           /* Current tool call name */
    char *current_tool_args;           /* Accumulated tool arguments */
};

/* ================================================================
 * Forward Declarations
 * ================================================================ */

static void emit_event(ik_openai_chat_stream_ctx_t *sctx, const ik_stream_event_t *event);
static void maybe_emit_start(ik_openai_chat_stream_ctx_t *sctx);
static void maybe_end_tool_call(ik_openai_chat_stream_ctx_t *sctx);
static void process_delta(ik_openai_chat_stream_ctx_t *sctx, yyjson_val *delta, const char *finish_reason_str);

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
 * Event Emission Helpers
 * ================================================================ */

/**
 * Emit a stream event to the user callback
 */
static void emit_event(ik_openai_chat_stream_ctx_t *sctx, const ik_stream_event_t *event)
{
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE
    assert(event != NULL); // LCOV_EXCL_BR_LINE

    sctx->stream_cb(event, sctx->stream_ctx);
}

/**
 * Emit IK_STREAM_START if not yet started
 */
static void maybe_emit_start(ik_openai_chat_stream_ctx_t *sctx)
{
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE

    if (!sctx->started) {
        ik_stream_event_t event = {
            .type = IK_STREAM_START,
            .index = 0,
            .data.start.model = sctx->model
        };
        emit_event(sctx, &event);
        sctx->started = true;
    }
}

/**
 * Emit IK_STREAM_TOOL_CALL_DONE if in a tool call
 */
static void maybe_end_tool_call(ik_openai_chat_stream_ctx_t *sctx)
{
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE

    if (sctx->in_tool_call) {
        ik_stream_event_t event = {
            .type = IK_STREAM_TOOL_CALL_DONE,
            .index = sctx->tool_call_index
        };
        emit_event(sctx, &event);
        sctx->in_tool_call = false;
    }
}

/* ================================================================
 * Delta Processing
 * ================================================================ */

/**
 * Process choices[0].delta object
 */
static void process_delta(ik_openai_chat_stream_ctx_t *sctx, yyjson_val *delta, const char *finish_reason_str)
{
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE
    assert(delta != NULL); // LCOV_EXCL_BR_LINE

    // Extract role (first chunk only)
    yyjson_val *role_val = yyjson_obj_get(delta, "role");
    if (role_val != NULL) {
        // First chunk with role - don't emit anything yet
        return;
    }

    // Extract content (text delta)
    yyjson_val *content_val = yyjson_obj_get(delta, "content");
    if (content_val != NULL && yyjson_is_str(content_val)) {
        const char *content = yyjson_get_str(content_val);
        if (content != NULL) {
            // End any active tool call before text
            maybe_end_tool_call(sctx);

            // Ensure START was emitted
            maybe_emit_start(sctx);

            // Emit text delta
            ik_stream_event_t event = {
                .type = IK_STREAM_TEXT_DELTA,
                .index = 0,
                .data.delta.text = content
            };
            emit_event(sctx, &event);
        }
    }

    // Extract tool_calls array
    yyjson_val *tool_calls_val = yyjson_obj_get(delta, "tool_calls");
    if (tool_calls_val != NULL && yyjson_is_arr(tool_calls_val)) {
        size_t arr_size = yyjson_arr_size(tool_calls_val);
        if (arr_size > 0) {
            // Get first tool call (index 0)
            yyjson_val *tool_call = yyjson_arr_get(tool_calls_val, 0);
            if (tool_call != NULL && yyjson_is_obj(tool_call)) {
                // Extract index
                yyjson_val *index_val = yyjson_obj_get(tool_call, "index");
                int32_t tc_index = 0;
                if (index_val != NULL && yyjson_is_int(index_val)) {
                    tc_index = (int32_t)yyjson_get_int(index_val);
                }

                // Check if this is a new tool call (different index)
                if (tc_index != sctx->tool_call_index) {
                    // End previous tool call if active
                    maybe_end_tool_call(sctx);

                    // Extract id and function.name for new tool call
                    yyjson_val *id_val = yyjson_obj_get(tool_call, "id");
                    yyjson_val *function_val = yyjson_obj_get(tool_call, "function");

                    if (id_val != NULL && function_val != NULL && yyjson_is_obj(function_val)) {
                        const char *id = yyjson_get_str(id_val);
                        yyjson_val *name_val = yyjson_obj_get(function_val, "name");
                        const char *name = yyjson_get_str(name_val);

                        if (id != NULL && name != NULL) {
                            // Ensure START was emitted
                            maybe_emit_start(sctx);

                            // Store tool call info
                            sctx->tool_call_index = tc_index;
                            sctx->current_tool_id = talloc_strdup(sctx, id);
                            if (sctx->current_tool_id == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
                            sctx->current_tool_name = talloc_strdup(sctx, name);
                            if (sctx->current_tool_name == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
                            sctx->current_tool_args = talloc_strdup(sctx, "");
                            if (sctx->current_tool_args == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

                            // Emit START event
                            ik_stream_event_t event = {
                                .type = IK_STREAM_TOOL_CALL_START,
                                .index = tc_index,
                                .data.tool_start.id = sctx->current_tool_id,
                                .data.tool_start.name = sctx->current_tool_name
                            };
                            emit_event(sctx, &event);
                            sctx->in_tool_call = true;
                        }
                    }
                }

                // Extract function.arguments delta
                yyjson_val *function_val = yyjson_obj_get(tool_call, "function");
                if (function_val != NULL && yyjson_is_obj(function_val)) {
                    yyjson_val *arguments_val = yyjson_obj_get(function_val, "arguments");
                    if (arguments_val != NULL && yyjson_is_str(arguments_val)) {
                        const char *arguments = yyjson_get_str(arguments_val);
                        if (arguments != NULL && sctx->in_tool_call) {
                            // Accumulate arguments
                            char *new_args = talloc_asprintf(sctx, "%s%s",
                                sctx->current_tool_args ? sctx->current_tool_args : "",
                                arguments);
                            if (new_args == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
                            talloc_free(sctx->current_tool_args);
                            sctx->current_tool_args = new_args;

                            // Emit DELTA event
                            ik_stream_event_t event = {
                                .type = IK_STREAM_TOOL_CALL_DELTA,
                                .index = tc_index,
                                .data.tool_delta.arguments = arguments
                            };
                            emit_event(sctx, &event);
                        }
                    }
                }
            }
        }
    }

    // Update finish_reason if provided
    if (finish_reason_str != NULL) {
        sctx->finish_reason = ik_openai_map_chat_finish_reason(finish_reason_str);
    }
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
        maybe_end_tool_call(stream_ctx);

        // Emit DONE event
        ik_stream_event_t event = {
            .type = IK_STREAM_DONE,
            .index = 0,
            .data.done.finish_reason = stream_ctx->finish_reason,
            .data.done.usage = stream_ctx->usage,
            .data.done.provider_data = NULL
        };
        emit_event(stream_ctx, &event);
        return;
    }

    // Parse JSON
    yyjson_doc *doc = yyjson_read(data, strlen(data), 0);
    if (doc == NULL) {
        // Malformed JSON - skip silently
        return;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (root == NULL || !yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return;
    }

    // Check for error
    yyjson_val *error_val = yyjson_obj_get(root, "error");
    if (error_val != NULL && yyjson_is_obj(error_val)) {
        // Extract error message and type
        yyjson_val *message_val = yyjson_obj_get(error_val, "message");
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
        emit_event(stream_ctx, &event);
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
            if (choice0 != NULL && yyjson_is_obj(choice0)) {
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
                    process_delta(stream_ctx, delta_val, finish_reason_str);
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
