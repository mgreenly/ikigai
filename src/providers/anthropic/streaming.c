/**
 * @file streaming.c
 * @brief Anthropic streaming implementation
 */

#include "streaming.h"
#include "response.h"
#include "request.h"
#include "json_allocator.h"
#include "panic.h"
#include "providers/common/http_multi.h"
#include "vendor/yyjson/yyjson.h"
#include <assert.h>
#include <string.h>

/**
 * Anthropic streaming context structure
 */
struct ik_anthropic_stream_ctx {
    ik_stream_cb_t stream_cb;          /* User's stream callback */
    void *stream_ctx;                  /* User's stream context */
    ik_sse_parser_t *sse_parser;       /* SSE parser instance */
    char *model;                       /* Model name from message_start */
    ik_finish_reason_t finish_reason;  /* Finish reason from message_delta */
    ik_usage_t usage;                  /* Accumulated usage statistics */
    int32_t current_block_index;       /* Current content block index */
    ik_content_type_t current_block_type; /* Current block type */
    char *current_tool_id;             /* Current tool call ID */
    char *current_tool_name;           /* Current tool call name */
};

/* ================================================================
 * Forward Declarations
 * ================================================================ */

static size_t curl_write_callback(const char *data, size_t len, void *ctx);

/* ================================================================
 * Context Creation
 * ================================================================ */

res_t ik_anthropic_stream_ctx_create(TALLOC_CTX *ctx, ik_stream_cb_t stream_cb,
                                      void *stream_ctx, ik_anthropic_stream_ctx_t **out)
{
    assert(ctx != NULL);        // LCOV_EXCL_BR_LINE
    assert(stream_cb != NULL);  // LCOV_EXCL_BR_LINE
    assert(out != NULL);        // LCOV_EXCL_BR_LINE

    // Allocate streaming context
    ik_anthropic_stream_ctx_t *sctx = talloc_zero(ctx, ik_anthropic_stream_ctx_t);
    if (sctx == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Store user callbacks
    sctx->stream_cb = stream_cb;
    sctx->stream_ctx = stream_ctx;

    // Create SSE parser with event callback
    sctx->sse_parser = ik_sse_parser_create(sctx);
    if (sctx->sse_parser == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Initialize state
    sctx->model = NULL;
    sctx->finish_reason = IK_FINISH_UNKNOWN;
    memset(&sctx->usage, 0, sizeof(ik_usage_t));
    sctx->current_block_index = -1;
    sctx->current_block_type = IK_CONTENT_TEXT;
    sctx->current_tool_id = NULL;
    sctx->current_tool_name = NULL;

    *out = sctx;
    return OK(sctx);
}

/* ================================================================
 * Getters
 * ================================================================ */

ik_usage_t ik_anthropic_stream_get_usage(ik_anthropic_stream_ctx_t *stream_ctx)
{
    assert(stream_ctx != NULL); // LCOV_EXCL_BR_LINE
    return stream_ctx->usage;
}

ik_finish_reason_t ik_anthropic_stream_get_finish_reason(ik_anthropic_stream_ctx_t *stream_ctx)
{
    assert(stream_ctx != NULL); // LCOV_EXCL_BR_LINE
    return stream_ctx->finish_reason;
}

/* ================================================================
 * Event Processing Helpers
 * ================================================================ */

/**
 * Process message_start event
 */
static void process_message_start(ik_anthropic_stream_ctx_t *sctx, yyjson_val *root)
{
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE
    assert(root != NULL); // LCOV_EXCL_BR_LINE

    // Extract model from message object
    yyjson_val *message_obj = yyjson_obj_get(root, "message");
    if (message_obj != NULL && yyjson_is_obj(message_obj)) {
        yyjson_val *model_val = yyjson_obj_get(message_obj, "model");
        if (model_val != NULL) {
            const char *model = yyjson_get_str(model_val);
            if (model != NULL) {
                sctx->model = talloc_strdup(sctx, model);
                if (sctx->model == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            }
        }

        // Extract initial usage (input_tokens)
        yyjson_val *usage_obj = yyjson_obj_get(message_obj, "usage");
        if (usage_obj != NULL && yyjson_is_obj(usage_obj)) {
            yyjson_val *input_val = yyjson_obj_get(usage_obj, "input_tokens");
            if (input_val != NULL && yyjson_is_int(input_val)) {
                sctx->usage.input_tokens = (int32_t)yyjson_get_int(input_val);
            }
        }
    }

    // Emit IK_STREAM_START event
    ik_stream_event_t event = {
        .type = IK_STREAM_START,
        .index = 0,
        .data.start.model = sctx->model
    };
    sctx->stream_cb(&event, sctx->stream_ctx);
}

/**
 * Process content_block_start event
 */
static void process_content_block_start(ik_anthropic_stream_ctx_t *sctx, yyjson_val *root)
{
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE
    assert(root != NULL); // LCOV_EXCL_BR_LINE

    // Extract index
    yyjson_val *index_val = yyjson_obj_get(root, "index");
    if (index_val != NULL && yyjson_is_int(index_val)) {
        sctx->current_block_index = (int32_t)yyjson_get_int(index_val);
    }

    // Extract content_block object
    yyjson_val *block_obj = yyjson_obj_get(root, "content_block");
    if (block_obj == NULL || !yyjson_is_obj(block_obj)) {
        return;
    }

    // Extract type
    yyjson_val *type_val = yyjson_obj_get(block_obj, "type");
    if (type_val == NULL) {
        return;
    }

    const char *type_str = yyjson_get_str(type_val);
    if (type_str == NULL) {
        return;
    }

    // Handle different block types
    if (strcmp(type_str, "text") == 0) {
        sctx->current_block_type = IK_CONTENT_TEXT;
        // No event emission for text blocks
    } else if (strcmp(type_str, "thinking") == 0) {
        sctx->current_block_type = IK_CONTENT_THINKING;
        // No event emission for thinking blocks
    } else if (strcmp(type_str, "tool_use") == 0) {
        sctx->current_block_type = IK_CONTENT_TOOL_CALL;

        // Extract id
        yyjson_val *id_val = yyjson_obj_get(block_obj, "id");
        if (id_val != NULL) {
            const char *id = yyjson_get_str(id_val);
            if (id != NULL) {
                sctx->current_tool_id = talloc_strdup(sctx, id);
                if (sctx->current_tool_id == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            }
        }

        // Extract name
        yyjson_val *name_val = yyjson_obj_get(block_obj, "name");
        if (name_val != NULL) {
            const char *name = yyjson_get_str(name_val);
            if (name != NULL) {
                sctx->current_tool_name = talloc_strdup(sctx, name);
                if (sctx->current_tool_name == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            }
        }

        // Emit IK_STREAM_TOOL_CALL_START
        ik_stream_event_t event = {
            .type = IK_STREAM_TOOL_CALL_START,
            .index = sctx->current_block_index,
            .data.tool_start.id = sctx->current_tool_id,
            .data.tool_start.name = sctx->current_tool_name
        };
        sctx->stream_cb(&event, sctx->stream_ctx);
    }
}

/**
 * Process content_block_delta event
 */
static void process_content_block_delta(ik_anthropic_stream_ctx_t *sctx, yyjson_val *root)
{
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE
    assert(root != NULL); // LCOV_EXCL_BR_LINE

    // Extract index
    yyjson_val *index_val = yyjson_obj_get(root, "index");
    int32_t index = 0;
    if (index_val != NULL && yyjson_is_int(index_val)) {
        index = (int32_t)yyjson_get_int(index_val);
    }

    // Extract delta object
    yyjson_val *delta_obj = yyjson_obj_get(root, "delta");
    if (delta_obj == NULL || !yyjson_is_obj(delta_obj)) {
        return;
    }

    // Extract delta type
    yyjson_val *type_val = yyjson_obj_get(delta_obj, "type");
    if (type_val == NULL) {
        return;
    }

    const char *type_str = yyjson_get_str(type_val);
    if (type_str == NULL) {
        return;
    }

    // Handle different delta types
    if (strcmp(type_str, "text_delta") == 0) {
        // Extract text
        yyjson_val *text_val = yyjson_obj_get(delta_obj, "text");
        if (text_val != NULL) {
            const char *text = yyjson_get_str(text_val);
            if (text != NULL) {
                // Emit IK_STREAM_TEXT_DELTA
                ik_stream_event_t event = {
                    .type = IK_STREAM_TEXT_DELTA,
                    .index = index,
                    .data.delta.text = text
                };
                sctx->stream_cb(&event, sctx->stream_ctx);
            }
        }
    } else if (strcmp(type_str, "thinking_delta") == 0) {
        // Extract thinking
        yyjson_val *thinking_val = yyjson_obj_get(delta_obj, "thinking");
        if (thinking_val != NULL) {
            const char *thinking = yyjson_get_str(thinking_val);
            if (thinking != NULL) {
                // Emit IK_STREAM_THINKING_DELTA
                ik_stream_event_t event = {
                    .type = IK_STREAM_THINKING_DELTA,
                    .index = index,
                    .data.delta.text = thinking
                };
                sctx->stream_cb(&event, sctx->stream_ctx);
            }
        }
    } else if (strcmp(type_str, "input_json_delta") == 0) {
        // Extract partial_json
        yyjson_val *json_val = yyjson_obj_get(delta_obj, "partial_json");
        if (json_val != NULL) {
            const char *partial_json = yyjson_get_str(json_val);
            if (partial_json != NULL) {
                // Emit IK_STREAM_TOOL_CALL_DELTA
                ik_stream_event_t event = {
                    .type = IK_STREAM_TOOL_CALL_DELTA,
                    .index = index,
                    .data.tool_delta.arguments = partial_json
                };
                sctx->stream_cb(&event, sctx->stream_ctx);
            }
        }
    }
}

/**
 * Process content_block_stop event
 */
static void process_content_block_stop(ik_anthropic_stream_ctx_t *sctx, yyjson_val *root)
{
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE
    assert(root != NULL); // LCOV_EXCL_BR_LINE

    // Extract index
    yyjson_val *index_val = yyjson_obj_get(root, "index");
    int32_t index = 0;
    if (index_val != NULL && yyjson_is_int(index_val)) {
        index = (int32_t)yyjson_get_int(index_val);
    }

    // Only emit TOOL_CALL_DONE for tool_use blocks
    if (sctx->current_block_type == IK_CONTENT_TOOL_CALL) {
        ik_stream_event_t event = {
            .type = IK_STREAM_TOOL_CALL_DONE,
            .index = index
        };
        sctx->stream_cb(&event, sctx->stream_ctx);
    }

    // Reset current block tracking
    sctx->current_block_index = -1;
}

/**
 * Process message_delta event
 */
static void process_message_delta(ik_anthropic_stream_ctx_t *sctx, yyjson_val *root)
{
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE
    assert(root != NULL); // LCOV_EXCL_BR_LINE

    // Extract delta object
    yyjson_val *delta_obj = yyjson_obj_get(root, "delta");
    if (delta_obj != NULL && yyjson_is_obj(delta_obj)) {
        // Extract stop_reason if present
        yyjson_val *stop_reason_val = yyjson_obj_get(delta_obj, "stop_reason");
        if (stop_reason_val != NULL) {
            const char *stop_reason = yyjson_get_str(stop_reason_val);
            if (stop_reason != NULL) {
                sctx->finish_reason = ik_anthropic_map_finish_reason(stop_reason);
            }
        }
    }

    // Extract usage object
    yyjson_val *usage_obj = yyjson_obj_get(root, "usage");
    if (usage_obj != NULL && yyjson_is_obj(usage_obj)) {
        // Extract output_tokens
        yyjson_val *output_val = yyjson_obj_get(usage_obj, "output_tokens");
        if (output_val != NULL && yyjson_is_int(output_val)) {
            sctx->usage.output_tokens = (int32_t)yyjson_get_int(output_val);
        }

        // Extract thinking_tokens
        yyjson_val *thinking_val = yyjson_obj_get(usage_obj, "thinking_tokens");
        if (thinking_val != NULL && yyjson_is_int(thinking_val)) {
            sctx->usage.thinking_tokens = (int32_t)yyjson_get_int(thinking_val);
        }

        // Calculate total_tokens
        sctx->usage.total_tokens = sctx->usage.input_tokens +
                                   sctx->usage.output_tokens +
                                   sctx->usage.thinking_tokens;
    }

    // No event emission for message_delta
}

/**
 * Process message_stop event
 */
static void process_message_stop(ik_anthropic_stream_ctx_t *sctx, yyjson_val *root)
{
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE
    (void)root; // message_stop has no useful data

    // Emit IK_STREAM_DONE with accumulated usage and finish_reason
    ik_stream_event_t event = {
        .type = IK_STREAM_DONE,
        .index = 0,
        .data.done.finish_reason = sctx->finish_reason,
        .data.done.usage = sctx->usage,
        .data.done.provider_data = NULL
    };
    sctx->stream_cb(&event, sctx->stream_ctx);
}

/**
 * Process error event
 */
static void process_error(ik_anthropic_stream_ctx_t *sctx, yyjson_val *root)
{
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE
    assert(root != NULL); // LCOV_EXCL_BR_LINE

    // Extract error object
    yyjson_val *error_obj = yyjson_obj_get(root, "error");
    if (error_obj == NULL || !yyjson_is_obj(error_obj)) {
        // Emit generic error
        ik_stream_event_t event = {
            .type = IK_STREAM_ERROR,
            .index = 0,
            .data.error.category = IK_ERR_CAT_UNKNOWN,
            .data.error.message = "Unknown error"
        };
        sctx->stream_cb(&event, sctx->stream_ctx);
        return;
    }

    // Extract error type
    const char *type_str = NULL;
    yyjson_val *type_val = yyjson_obj_get(error_obj, "type");
    if (type_val != NULL) {
        type_str = yyjson_get_str(type_val);
    }

    // Extract error message
    const char *message = "Unknown error";
    yyjson_val *msg_val = yyjson_obj_get(error_obj, "message");
    if (msg_val != NULL) {
        const char *msg = yyjson_get_str(msg_val);
        if (msg != NULL) {
            message = msg;
        }
    }

    // Map error type to category
    ik_error_category_t category = IK_ERR_CAT_UNKNOWN;
    if (type_str != NULL) {
        if (strcmp(type_str, "authentication_error") == 0) {
            category = IK_ERR_CAT_AUTH;
        } else if (strcmp(type_str, "rate_limit_error") == 0) {
            category = IK_ERR_CAT_RATE_LIMIT;
        } else if (strcmp(type_str, "overloaded_error") == 0) {
            category = IK_ERR_CAT_SERVER;
        } else if (strcmp(type_str, "invalid_request_error") == 0) {
            category = IK_ERR_CAT_INVALID_ARG;
        }
    }

    // Emit IK_STREAM_ERROR
    ik_stream_event_t event = {
        .type = IK_STREAM_ERROR,
        .index = 0,
        .data.error.category = category,
        .data.error.message = message
    };
    sctx->stream_cb(&event, sctx->stream_ctx);
}

/* ================================================================
 * Main Event Processing
 * ================================================================ */

void ik_anthropic_stream_process_event(ik_anthropic_stream_ctx_t *stream_ctx,
                                        const char *event, const char *data)
{
    assert(stream_ctx != NULL); // LCOV_EXCL_BR_LINE
    assert(event != NULL);      // LCOV_EXCL_BR_LINE
    assert(data != NULL);       // LCOV_EXCL_BR_LINE

    // Ignore ping events
    if (strcmp(event, "ping") == 0) {
        return;
    }

    // Parse JSON data
    yyjson_alc allocator = ik_make_talloc_allocator(stream_ctx);
    yyjson_doc *doc = yyjson_read_opts((char *)(void *)(size_t)(const void *)data,
                                        strlen(data), 0, &allocator, NULL);
    if (doc == NULL) {
        // Invalid JSON - emit error
        ik_stream_event_t error_event = {
            .type = IK_STREAM_ERROR,
            .index = 0,
            .data.error.category = IK_ERR_CAT_UNKNOWN,
            .data.error.message = "Invalid JSON in SSE event"
        };
        stream_ctx->stream_cb(&error_event, stream_ctx->stream_ctx);
        return;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        // Not an object - emit error
        ik_stream_event_t error_event = {
            .type = IK_STREAM_ERROR,
            .index = 0,
            .data.error.category = IK_ERR_CAT_UNKNOWN,
            .data.error.message = "SSE event data is not a JSON object"
        };
        stream_ctx->stream_cb(&error_event, stream_ctx->stream_ctx);
        return;
    }

    // Dispatch based on event type
    if (strcmp(event, "message_start") == 0) {
        process_message_start(stream_ctx, root);
    } else if (strcmp(event, "content_block_start") == 0) {
        process_content_block_start(stream_ctx, root);
    } else if (strcmp(event, "content_block_delta") == 0) {
        process_content_block_delta(stream_ctx, root);
    } else if (strcmp(event, "content_block_stop") == 0) {
        process_content_block_stop(stream_ctx, root);
    } else if (strcmp(event, "message_delta") == 0) {
        process_message_delta(stream_ctx, root);
    } else if (strcmp(event, "message_stop") == 0) {
        process_message_stop(stream_ctx, root);
    } else if (strcmp(event, "error") == 0) {
        process_error(stream_ctx, root);
    }
    // Unknown events are ignored
}

/* ================================================================
 * Curl Write Callback
 * ================================================================ */

/**
 * Curl write callback for streaming responses
 *
 * Called by curl as data arrives from network.
 * Feeds data to SSE parser and pulls complete events.
 */
__attribute__((unused))
static size_t curl_write_callback(const char *data, size_t len, void *ctx)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE
    assert(data != NULL); // LCOV_EXCL_BR_LINE

    ik_anthropic_stream_ctx_t *stream_ctx = (ik_anthropic_stream_ctx_t *)ctx;

    // Feed data to SSE parser
    ik_sse_parser_feed(stream_ctx->sse_parser, data, len);

    // Pull and process all complete events
    ik_sse_event_t *event;
    while ((event = ik_sse_parser_next(stream_ctx->sse_parser, stream_ctx)) != NULL) {
        // Process the event
        const char *event_type = event->event ? event->event : "";
        const char *event_data = event->data ? event->data : "";

        ik_anthropic_stream_process_event(stream_ctx, event_type, event_data);

        // Event is allocated on stream_ctx, will be cleaned up automatically
        talloc_free(event);
    }

    // Return len to indicate all bytes consumed
    return len;
}
