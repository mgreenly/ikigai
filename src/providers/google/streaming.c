/**
 * @file streaming.c
 * @brief Google streaming implementation
 */

#include "streaming.h"
#include "response.h"
#include "json_allocator.h"
#include "panic.h"
#include "vendor/yyjson/yyjson.h"
#include <assert.h>
#include <string.h>

/**
 * Google streaming context structure
 */
struct ik_google_stream_ctx {
    ik_stream_cb_t user_cb;            /* User's stream callback */
    void *user_ctx;                    /* User's stream context */
    char *model;                       /* Model name from modelVersion */
    ik_finish_reason_t finish_reason;  /* Finish reason from finishReason */
    ik_usage_t usage;                  /* Accumulated usage statistics */
    bool started;                      /* true after IK_STREAM_START emitted */
    bool in_thinking;                  /* true when processing thinking content */
    bool in_tool_call;                 /* true when processing tool call */
    char *current_tool_id;             /* Current tool call ID (generated) */
    char *current_tool_name;           /* Current tool call name */
    int32_t part_index;                /* Current part index for events */
};

/* ================================================================
 * Helper Functions
 * ================================================================ */

/**
 * End any open tool call
 */
static void end_tool_call_if_needed(ik_google_stream_ctx_t *sctx)
{
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE

    if (sctx->in_tool_call) {
        ik_stream_event_t event = {
            .type = IK_STREAM_TOOL_CALL_DONE,
            .index = sctx->part_index
        };
        sctx->user_cb(&event, sctx->user_ctx);
        sctx->in_tool_call = false;
        sctx->current_tool_id = NULL;
        sctx->current_tool_name = NULL;
    }
}

/**
 * Map Google status string to error category
 */
static ik_error_category_t map_error_status(const char *status)
{
    if (status == NULL) {
        return IK_ERR_CAT_UNKNOWN;
    }

    if (strcmp(status, "UNAUTHENTICATED") == 0) {
        return IK_ERR_CAT_AUTH;
    } else if (strcmp(status, "RESOURCE_EXHAUSTED") == 0) {
        return IK_ERR_CAT_RATE_LIMIT;
    } else if (strcmp(status, "INVALID_ARGUMENT") == 0) {
        return IK_ERR_CAT_INVALID_ARG;
    }

    return IK_ERR_CAT_UNKNOWN;
}

/**
 * Process error object from chunk
 */
static void process_error(ik_google_stream_ctx_t *sctx, yyjson_val *error_obj)
{
    assert(sctx != NULL);     // LCOV_EXCL_BR_LINE
    assert(error_obj != NULL); // LCOV_EXCL_BR_LINE

    // Extract message
    const char *message = "Unknown error";
    yyjson_val *msg_val = yyjson_obj_get(error_obj, "message");
    if (msg_val != NULL) {
        const char *msg_str = yyjson_get_str(msg_val);
        if (msg_str != NULL) {
            message = msg_str;
        }
    }

    // Extract status for category mapping
    ik_error_category_t category = IK_ERR_CAT_UNKNOWN;
    yyjson_val *status_val = yyjson_obj_get(error_obj, "status");
    if (status_val != NULL) {
        const char *status_str = yyjson_get_str(status_val);
        category = map_error_status(status_str);
    }

    // Emit error event
    ik_stream_event_t event = {
        .type = IK_STREAM_ERROR,
        .index = 0,
        .data.error.category = category,
        .data.error.message = message
    };
    sctx->user_cb(&event, sctx->user_ctx);
}

/**
 * Process functionCall part
 */
static void process_function_call(ik_google_stream_ctx_t *sctx, yyjson_val *function_call)
{
    assert(sctx != NULL);          // LCOV_EXCL_BR_LINE
    assert(function_call != NULL); // LCOV_EXCL_BR_LINE

    // If not already in a tool call, start one
    if (!sctx->in_tool_call) {
        // Generate tool call ID (22-char base64url)
        sctx->current_tool_id = ik_google_generate_tool_id(sctx);
        if (sctx->current_tool_id == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        // Extract function name
        yyjson_val *name_val = yyjson_obj_get(function_call, "name");
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
            .index = sctx->part_index,
            .data.tool_start.id = sctx->current_tool_id,
            .data.tool_start.name = sctx->current_tool_name
        };
        sctx->user_cb(&event, sctx->user_ctx);
        sctx->in_tool_call = true;
    }

    // Extract and emit arguments
    yyjson_val *args_val = yyjson_obj_get(function_call, "args");
    if (args_val != NULL) {
        // Serialize args to JSON string
        yyjson_write_flag flg = YYJSON_WRITE_NOFLAG;
        size_t json_len;
        char *args_json = yyjson_val_write_opts(args_val, flg, NULL, &json_len, NULL);
        if (args_json != NULL) {
            // Emit IK_STREAM_TOOL_CALL_DELTA
            ik_stream_event_t event = {
                .type = IK_STREAM_TOOL_CALL_DELTA,
                .index = sctx->part_index,
                .data.tool_delta.arguments = args_json
            };
            sctx->user_cb(&event, sctx->user_ctx);
            free(args_json); // yyjson uses malloc
        }
    }
}

/**
 * Process thinking part (thought=true)
 */
static void process_thinking_part(ik_google_stream_ctx_t *sctx, const char *text)
{
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE
    assert(text != NULL); // LCOV_EXCL_BR_LINE

    // End any open tool call first
    end_tool_call_if_needed(sctx);

    // Set thinking state
    sctx->in_thinking = true;

    // Emit IK_STREAM_THINKING_DELTA
    ik_stream_event_t event = {
        .type = IK_STREAM_THINKING_DELTA,
        .index = sctx->part_index,
        .data.delta.text = text
    };
    sctx->user_cb(&event, sctx->user_ctx);
}

/**
 * Process regular text part
 */
static void process_text_part(ik_google_stream_ctx_t *sctx, const char *text)
{
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE
    assert(text != NULL); // LCOV_EXCL_BR_LINE

    // End any open tool call first
    end_tool_call_if_needed(sctx);

    // If transitioning from thinking, increment part index
    if (sctx->in_thinking) {
        sctx->part_index++;
        sctx->in_thinking = false;
    }

    // Emit IK_STREAM_TEXT_DELTA
    ik_stream_event_t event = {
        .type = IK_STREAM_TEXT_DELTA,
        .index = sctx->part_index,
        .data.delta.text = text
    };
    sctx->user_cb(&event, sctx->user_ctx);
}

/**
 * Process content parts array
 */
static void process_parts(ik_google_stream_ctx_t *sctx, yyjson_val *parts_arr)
{
    assert(sctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(parts_arr != NULL); // LCOV_EXCL_BR_LINE

    size_t idx, max;
    yyjson_val *part;
    yyjson_arr_foreach(parts_arr, idx, max, part) {
        // Check for functionCall
        yyjson_val *function_call = yyjson_obj_get(part, "functionCall");
        if (function_call != NULL) {
            process_function_call(sctx, function_call);
            continue;
        }

        // Check for thought flag
        yyjson_val *thought_val = yyjson_obj_get(part, "thought");
        bool is_thought = thought_val != NULL && yyjson_get_bool(thought_val);

        // Extract text
        yyjson_val *text_val = yyjson_obj_get(part, "text");
        if (text_val == NULL) {
            // Skip parts without text or functionCall
            continue;
        }

        const char *text = yyjson_get_str(text_val);
        if (text == NULL || text[0] == '\0') {
            // Skip empty text
            continue;
        }

        if (is_thought) {
            process_thinking_part(sctx, text);
        } else {
            process_text_part(sctx, text);
        }
    }
}

/**
 * Process usage metadata
 */
static void process_usage(ik_google_stream_ctx_t *sctx, yyjson_val *usage_obj)
{
    assert(sctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(usage_obj != NULL); // LCOV_EXCL_BR_LINE

    yyjson_val *prompt_tokens = yyjson_obj_get(usage_obj, "promptTokenCount");
    yyjson_val *candidates_tokens = yyjson_obj_get(usage_obj, "candidatesTokenCount");
    yyjson_val *thoughts_tokens = yyjson_obj_get(usage_obj, "thoughtsTokenCount");
    yyjson_val *total_tokens = yyjson_obj_get(usage_obj, "totalTokenCount");

    int32_t prompt = prompt_tokens ? (int32_t)yyjson_get_int(prompt_tokens) : 0;
    int32_t candidates = candidates_tokens ? (int32_t)yyjson_get_int(candidates_tokens) : 0;
    int32_t thoughts = thoughts_tokens ? (int32_t)yyjson_get_int(thoughts_tokens) : 0;

    sctx->usage.input_tokens = prompt;
    sctx->usage.thinking_tokens = thoughts;
    sctx->usage.output_tokens = candidates - thoughts; // Exclude thinking from output
    sctx->usage.total_tokens = total_tokens ? (int32_t)yyjson_get_int(total_tokens) : 0;
    sctx->usage.cached_tokens = 0; // Google doesn't report cache hits

    // End any open tool call
    end_tool_call_if_needed(sctx);

    // Emit IK_STREAM_DONE
    ik_stream_event_t event = {
        .type = IK_STREAM_DONE,
        .index = 0,
        .data.done.finish_reason = sctx->finish_reason,
        .data.done.usage = sctx->usage,
        .data.done.provider_data = NULL
    };
    sctx->user_cb(&event, sctx->user_ctx);
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

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return;
    }

    // Check for error object first
    yyjson_val *error_obj = yyjson_obj_get(root, "error");
    if (error_obj != NULL) {
        process_error(stream_ctx, error_obj);
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
                    process_parts(stream_ctx, parts);
                }
            }
        }
    }

    // Extract usage metadata (signals end of stream)
    yyjson_val *usage_obj = yyjson_obj_get(root, "usageMetadata");
    if (usage_obj != NULL) {
        process_usage(stream_ctx, usage_obj);
    }

    yyjson_doc_free(doc);
}
