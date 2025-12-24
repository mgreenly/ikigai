/**
 * @file streaming_responses.c
 * @brief OpenAI Responses API streaming implementation
 */

#include "streaming.h"
#include "response.h"
#include "error.h"
#include "panic.h"
#include "providers/common/sse_parser.h"
#include "vendor/yyjson/yyjson.h"
#include <assert.h>
#include <string.h>

/**
 * OpenAI Responses API streaming context structure
 */
struct ik_openai_responses_stream_ctx {
    ik_stream_cb_t stream_cb;          /* User's stream callback */
    void *stream_ctx;                  /* User's stream context */
    char *model;                       /* Model name from response.created */
    ik_finish_reason_t finish_reason;  /* Finish reason from status */
    ik_usage_t usage;                  /* Accumulated usage statistics */
    bool started;                      /* Whether IK_STREAM_START was emitted */
    bool in_tool_call;                 /* Whether currently in a tool call */
    int32_t tool_call_index;           /* Current tool call index (output_index) */
    char *current_tool_id;             /* Current tool call ID */
    char *current_tool_name;           /* Current tool call name */
    ik_sse_parser_t *sse_parser;       /* SSE parser for processing chunks */
};

/* ================================================================
 * Forward Declarations
 * ================================================================ */

static void emit_event(ik_openai_responses_stream_ctx_t *sctx, const ik_stream_event_t *event);
static void maybe_emit_start(ik_openai_responses_stream_ctx_t *sctx);
static void maybe_end_tool_call(ik_openai_responses_stream_ctx_t *sctx);
static void parse_usage(yyjson_val *usage_val, ik_usage_t *out_usage);
static void sse_event_handler(const char *event_name, const char *data, size_t len, void *user_ctx);

/* ================================================================
 * Context Creation
 * ================================================================ */

ik_openai_responses_stream_ctx_t *ik_openai_responses_stream_ctx_create(TALLOC_CTX *ctx,
                                                                          ik_stream_cb_t stream_cb,
                                                                          void *stream_ctx)
{
    assert(ctx != NULL);        // LCOV_EXCL_BR_LINE
    assert(stream_cb != NULL);  // LCOV_EXCL_BR_LINE

    // Allocate streaming context
    ik_openai_responses_stream_ctx_t *sctx = talloc_zero(ctx, ik_openai_responses_stream_ctx_t);
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

    // Create SSE parser
    sctx->sse_parser = ik_sse_parser_create(sctx);
    if (sctx->sse_parser == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    return sctx;
}

/* ================================================================
 * Getters
 * ================================================================ */

ik_usage_t ik_openai_responses_stream_get_usage(ik_openai_responses_stream_ctx_t *stream_ctx)
{
    assert(stream_ctx != NULL); // LCOV_EXCL_BR_LINE
    return stream_ctx->usage;
}

ik_finish_reason_t ik_openai_responses_stream_get_finish_reason(ik_openai_responses_stream_ctx_t *stream_ctx)
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
static void emit_event(ik_openai_responses_stream_ctx_t *sctx, const ik_stream_event_t *event)
{
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE
    assert(event != NULL); // LCOV_EXCL_BR_LINE

    sctx->stream_cb(event, sctx->stream_ctx);
}

/**
 * Emit IK_STREAM_START if not yet started
 */
static void maybe_emit_start(ik_openai_responses_stream_ctx_t *sctx)
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
static void maybe_end_tool_call(ik_openai_responses_stream_ctx_t *sctx)
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
 * Usage Parsing
 * ================================================================ */

/**
 * Parse usage object from JSON
 */
static void parse_usage(yyjson_val *usage_val, ik_usage_t *out_usage)
{
    assert(usage_val != NULL); // LCOV_EXCL_BR_LINE
    assert(out_usage != NULL); // LCOV_EXCL_BR_LINE

    if (!yyjson_is_obj(usage_val)) {
        return;
    }

    // Extract input_tokens
    yyjson_val *input_tokens_val = yyjson_obj_get(usage_val, "input_tokens");
    if (input_tokens_val != NULL && yyjson_is_int(input_tokens_val)) {
        out_usage->input_tokens = (int32_t)yyjson_get_int(input_tokens_val);
    }

    // Extract output_tokens
    yyjson_val *output_tokens_val = yyjson_obj_get(usage_val, "output_tokens");
    if (output_tokens_val != NULL && yyjson_is_int(output_tokens_val)) {
        out_usage->output_tokens = (int32_t)yyjson_get_int(output_tokens_val);
    }

    // Extract total_tokens
    yyjson_val *total_tokens_val = yyjson_obj_get(usage_val, "total_tokens");
    if (total_tokens_val != NULL && yyjson_is_int(total_tokens_val)) {
        out_usage->total_tokens = (int32_t)yyjson_get_int(total_tokens_val);
    } else if (out_usage->input_tokens > 0 || out_usage->output_tokens > 0) {
        // Calculate total if not provided
        out_usage->total_tokens = out_usage->input_tokens + out_usage->output_tokens;
    }

    // Extract output_tokens_details.reasoning_tokens (thinking tokens)
    yyjson_val *details_val = yyjson_obj_get(usage_val, "output_tokens_details");
    if (details_val != NULL && yyjson_is_obj(details_val)) {
        yyjson_val *reasoning_tokens_val = yyjson_obj_get(details_val, "reasoning_tokens");
        if (reasoning_tokens_val != NULL && yyjson_is_int(reasoning_tokens_val)) {
            out_usage->thinking_tokens = (int32_t)yyjson_get_int(reasoning_tokens_val);
        }
    }
}

/* ================================================================
 * Event Processing
 * ================================================================ */

/**
 * Process single SSE event
 */
void ik_openai_responses_stream_process_event(ik_openai_responses_stream_ctx_t *stream_ctx,
                                                const char *event_name,
                                                const char *data)
{
    assert(stream_ctx != NULL); // LCOV_EXCL_BR_LINE
    assert(event_name != NULL); // LCOV_EXCL_BR_LINE
    assert(data != NULL); // LCOV_EXCL_BR_LINE

    // Parse JSON data
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

    // Handle different event types
    if (strcmp(event_name, "response.created") == 0) {
        // Extract model from response.model
        yyjson_val *response_val = yyjson_obj_get(root, "response");
        if (response_val != NULL && yyjson_is_obj(response_val)) {
            yyjson_val *model_val = yyjson_obj_get(response_val, "model");
            if (model_val != NULL) {
                const char *model = yyjson_get_str(model_val);
                if (model != NULL) {
                    stream_ctx->model = talloc_strdup(stream_ctx, model);
                    if (stream_ctx->model == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
                }
            }
        }

        // Emit START event
        maybe_emit_start(stream_ctx);
    }
    else if (strcmp(event_name, "response.output_text.delta") == 0) {
        // Extract delta field
        yyjson_val *delta_val = yyjson_obj_get(root, "delta");
        if (delta_val != NULL && yyjson_is_str(delta_val)) {
            const char *delta = yyjson_get_str(delta_val);
            if (delta != NULL) {
                // Extract content_index
                yyjson_val *content_index_val = yyjson_obj_get(root, "content_index");
                int32_t content_index = 0;
                if (content_index_val != NULL && yyjson_is_int(content_index_val)) {
                    content_index = (int32_t)yyjson_get_int(content_index_val);
                }

                // Ensure START was emitted
                maybe_emit_start(stream_ctx);

                // Emit text delta
                ik_stream_event_t event = {
                    .type = IK_STREAM_TEXT_DELTA,
                    .index = content_index,
                    .data.delta.text = delta
                };
                emit_event(stream_ctx, &event);
            }
        }
    }
    else if (strcmp(event_name, "response.reasoning_summary_text.delta") == 0) {
        // Extract delta field (thinking text)
        yyjson_val *delta_val = yyjson_obj_get(root, "delta");
        if (delta_val != NULL && yyjson_is_str(delta_val)) {
            const char *delta = yyjson_get_str(delta_val);
            if (delta != NULL) {
                // Extract summary_index
                yyjson_val *summary_index_val = yyjson_obj_get(root, "summary_index");
                int32_t summary_index = 0;
                if (summary_index_val != NULL && yyjson_is_int(summary_index_val)) {
                    summary_index = (int32_t)yyjson_get_int(summary_index_val);
                }

                // Ensure START was emitted
                maybe_emit_start(stream_ctx);

                // Emit thinking delta
                ik_stream_event_t event = {
                    .type = IK_STREAM_THINKING_DELTA,
                    .index = summary_index,
                    .data.delta.text = delta
                };
                emit_event(stream_ctx, &event);
            }
        }
    }
    else if (strcmp(event_name, "response.output_item.added") == 0) {
        // Extract item object
        yyjson_val *item_val = yyjson_obj_get(root, "item");
        if (item_val != NULL && yyjson_is_obj(item_val)) {
            // Check item type
            yyjson_val *type_val = yyjson_obj_get(item_val, "type");
            const char *item_type = yyjson_get_str(type_val);

            if (item_type != NULL && strcmp(item_type, "function_call") == 0) {
                // Extract output_index
                yyjson_val *output_index_val = yyjson_obj_get(root, "output_index");
                int32_t output_index = 0;
                if (output_index_val != NULL && yyjson_is_int(output_index_val)) {
                    output_index = (int32_t)yyjson_get_int(output_index_val);
                }

                // Extract call_id and name
                yyjson_val *call_id_val = yyjson_obj_get(item_val, "call_id");
                yyjson_val *name_val = yyjson_obj_get(item_val, "name");

                const char *call_id = yyjson_get_str(call_id_val);
                const char *name = yyjson_get_str(name_val);

                if (call_id != NULL && name != NULL) {
                    // End previous tool call if active
                    maybe_end_tool_call(stream_ctx);

                    // Ensure START was emitted
                    maybe_emit_start(stream_ctx);

                    // Store tool call info
                    stream_ctx->tool_call_index = output_index;
                    stream_ctx->current_tool_id = talloc_strdup(stream_ctx, call_id);
                    if (stream_ctx->current_tool_id == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
                    stream_ctx->current_tool_name = talloc_strdup(stream_ctx, name);
                    if (stream_ctx->current_tool_name == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

                    // Emit TOOL_CALL_START event
                    ik_stream_event_t event = {
                        .type = IK_STREAM_TOOL_CALL_START,
                        .index = output_index,
                        .data.tool_start.id = stream_ctx->current_tool_id,
                        .data.tool_start.name = stream_ctx->current_tool_name
                    };
                    emit_event(stream_ctx, &event);
                    stream_ctx->in_tool_call = true;
                }
            }
            // If type is "message", ignore (text will come via output_text.delta)
        }
    }
    else if (strcmp(event_name, "response.function_call_arguments.delta") == 0) {
        // Extract delta field (JSON fragment)
        yyjson_val *delta_val = yyjson_obj_get(root, "delta");
        if (delta_val != NULL && yyjson_is_str(delta_val)) {
            const char *delta = yyjson_get_str(delta_val);
            if (delta != NULL && stream_ctx->in_tool_call) {
                // Extract output_index
                yyjson_val *output_index_val = yyjson_obj_get(root, "output_index");
                int32_t output_index = stream_ctx->tool_call_index;
                if (output_index_val != NULL && yyjson_is_int(output_index_val)) {
                    output_index = (int32_t)yyjson_get_int(output_index_val);
                }

                // Emit TOOL_CALL_DELTA event
                ik_stream_event_t event = {
                    .type = IK_STREAM_TOOL_CALL_DELTA,
                    .index = output_index,
                    .data.tool_delta.arguments = delta
                };
                emit_event(stream_ctx, &event);
            }
        }
    }
    else if (strcmp(event_name, "response.function_call_arguments.done") == 0) {
        // This event provides the complete arguments for verification
        // We don't need to do anything here - DONE event comes on output_item.done
    }
    else if (strcmp(event_name, "response.output_item.done") == 0) {
        // Check if this completes a tool call
        yyjson_val *output_index_val = yyjson_obj_get(root, "output_index");
        int32_t output_index = -1;
        if (output_index_val != NULL && yyjson_is_int(output_index_val)) {
            output_index = (int32_t)yyjson_get_int(output_index_val);
        }

        // If this matches our current tool call index, emit DONE
        if (stream_ctx->in_tool_call && output_index == stream_ctx->tool_call_index) {
            ik_stream_event_t event = {
                .type = IK_STREAM_TOOL_CALL_DONE,
                .index = output_index
            };
            emit_event(stream_ctx, &event);
            stream_ctx->in_tool_call = false;

            // Clear tool call state
            stream_ctx->current_tool_id = NULL;
            stream_ctx->current_tool_name = NULL;
        }
    }
    else if (strcmp(event_name, "response.completed") == 0) {
        // End any active tool call
        maybe_end_tool_call(stream_ctx);

        // Extract response object
        yyjson_val *response_val = yyjson_obj_get(root, "response");
        if (response_val != NULL && yyjson_is_obj(response_val)) {
            // Extract status
            yyjson_val *status_val = yyjson_obj_get(response_val, "status");
            const char *status = yyjson_get_str(status_val);

            // Extract incomplete_reason if present
            const char *incomplete_reason = NULL;
            yyjson_val *incomplete_details_val = yyjson_obj_get(response_val, "incomplete_details");
            if (incomplete_details_val != NULL && yyjson_is_obj(incomplete_details_val)) {
                yyjson_val *reason_val = yyjson_obj_get(incomplete_details_val, "reason");
                incomplete_reason = yyjson_get_str(reason_val);
            }

            // Map status to finish reason
            if (status != NULL) {
                stream_ctx->finish_reason = ik_openai_map_responses_status(status, incomplete_reason);
            }

            // Extract usage
            yyjson_val *usage_val = yyjson_obj_get(response_val, "usage");
            if (usage_val != NULL) {
                parse_usage(usage_val, &stream_ctx->usage);
            }
        }

        // Emit DONE event
        ik_stream_event_t event = {
            .type = IK_STREAM_DONE,
            .index = 0,
            .data.done.finish_reason = stream_ctx->finish_reason,
            .data.done.usage = stream_ctx->usage,
            .data.done.provider_data = NULL
        };
        emit_event(stream_ctx, &event);
    }
    else if (strcmp(event_name, "error") == 0) {
        // Extract error object
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
                if (strcmp(type, "authentication_error") == 0) {
                    category = IK_ERR_CAT_AUTH;
                } else if (strcmp(type, "rate_limit_error") == 0) {
                    category = IK_ERR_CAT_RATE_LIMIT;
                } else if (strcmp(type, "invalid_request_error") == 0) {
                    category = IK_ERR_CAT_INVALID_ARG;
                } else if (strcmp(type, "server_error") == 0) {
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
        }
    }
    // Ignore other event types (response.in_progress, response.content_part.added, etc.)

    yyjson_doc_free(doc);
}

/* ================================================================
 * SSE Parser Callback
 * ================================================================ */

/**
 * SSE event handler - routes events to process_event
 */
static void sse_event_handler(const char *event_name, const char *data, size_t len, void *user_ctx)
{
    assert(user_ctx != NULL); // LCOV_EXCL_BR_LINE
    (void)len;  // Unused - data is null-terminated

    ik_openai_responses_stream_ctx_t *stream_ctx = (ik_openai_responses_stream_ctx_t *)user_ctx;

    // Process the event
    ik_openai_responses_stream_process_event(stream_ctx, event_name, data);
}

/* ================================================================
 * Write Callback
 * ================================================================ */

/**
 * Curl write callback for Responses API streaming
 */
size_t ik_openai_responses_stream_write_callback(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    assert(userdata != NULL); // LCOV_EXCL_BR_LINE

    ik_openai_responses_stream_ctx_t *ctx = (ik_openai_responses_stream_ctx_t *)userdata;
    size_t total = size * nmemb;

    // Feed data to SSE parser
    ik_sse_parser_feed(ctx->sse_parser, (const char *)ptr, total);

    // Extract and process complete events
    TALLOC_CTX *tmp_ctx = talloc_new(NULL);
    if (tmp_ctx == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    ik_sse_event_t *event;
    while ((event = ik_sse_parser_next(ctx->sse_parser, tmp_ctx)) != NULL) {
        // Process this event
        if (event->event != NULL && event->data != NULL) {
            sse_event_handler(event->event, event->data, strlen(event->data), ctx);
        }
        // Free this event before getting next one
        talloc_free(event);
    }

    talloc_free(tmp_ctx);

    return total;  // Return bytes consumed
}
