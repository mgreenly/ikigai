/**
 * @file streaming_helpers.c
 * @brief Google streaming helper functions
 */

#include "streaming_helpers.h"
#include "streaming_internal.h"
#include "response.h"
#include "json_allocator.h"
#include "panic.h"
#include "vendor/yyjson/yyjson.h"
#include <assert.h>
#include <string.h>

/* ================================================================
 * Helper Functions
 * ================================================================ */

/**
 * End any open tool call
 *
 * NOTE: This function marks the tool call as complete but preserves the
 * accumulated tool data (id, name, args) for the response builder.
 */
void ik_google_stream_end_tool_call_if_needed(ik_google_stream_ctx_t *sctx)
{
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE

    if (sctx->in_tool_call) {
        ik_stream_event_t event = {
            .type = IK_STREAM_TOOL_CALL_DONE,
            .index = sctx->part_index
        };
        sctx->user_cb(&event, sctx->user_ctx);
        sctx->in_tool_call = false;
        // Do NOT clear tool data here - it's needed by the response builder
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
void ik_google_stream_process_error(ik_google_stream_ctx_t *sctx, yyjson_val *error_obj)
{
    assert(sctx != NULL);     // LCOV_EXCL_BR_LINE
    assert(error_obj != NULL); // LCOV_EXCL_BR_LINE

    // Extract message
    const char *message = "Unknown error"; // LCOV_EXCL_BR_LINE (compiler artifact from yyjson_obj_get on next line)
    char *message_copy = NULL; // LCOV_EXCL_BR_LINE (unexecutable initialization)
    yyjson_val *msg_val = yyjson_obj_get(error_obj, "message");
    if (msg_val != NULL) {
        const char *msg_str = yyjson_get_str(msg_val);
        if (msg_str != NULL) {
            // Copy message before doc is freed
            message_copy = talloc_strdup(sctx, msg_str);
            if (message_copy == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            message = message_copy;
        }
    }

    // Extract status for category mapping
    ik_error_category_t category = IK_ERR_CAT_UNKNOWN; // LCOV_EXCL_BR_LINE (compiler artifact from yyjson_obj_get on next line)
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

    // message_copy is parented to sctx - it will be freed when sctx is freed
    // Do not free it here as callbacks may store a reference to the message
    (void)message_copy;
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

        // Initialize arguments accumulator
        sctx->current_tool_args = talloc_strdup(sctx, "");
        if (sctx->current_tool_args == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

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
        if (args_json != NULL) { // LCOV_EXCL_BR_LINE (only fails on extreme OOM, event skipped)
            // Accumulate arguments
            char *new_args = talloc_asprintf(sctx, "%s%s",
                                             sctx->current_tool_args ? sctx->current_tool_args : "", // LCOV_EXCL_BR_LINE
                                             args_json);
            if (new_args == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            talloc_free(sctx->current_tool_args);
            sctx->current_tool_args = new_args;

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
    ik_google_stream_end_tool_call_if_needed(sctx);

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
    ik_google_stream_end_tool_call_if_needed(sctx);

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
void ik_google_stream_process_parts(ik_google_stream_ctx_t *sctx, yyjson_val *parts_arr)
{
    assert(sctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(parts_arr != NULL); // LCOV_EXCL_BR_LINE

    size_t idx, max;
    yyjson_val *part;
    yyjson_arr_foreach(parts_arr, idx, max, part) { // LCOV_EXCL_BR_LINE (vendor macro loop control branches)
        // Check for functionCall
        yyjson_val *function_call = yyjson_obj_get(part, "functionCall");
        if (function_call != NULL) {
            process_function_call(sctx, function_call);
            continue;
        }

        // Check for thought flag
        yyjson_val *thought_val = yyjson_obj_get(part, "thought");
        bool is_thought = thought_val != NULL && yyjson_get_bool(thought_val); // LCOV_EXCL_BR_LINE (vendor macro bool type check)

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
void ik_google_stream_process_usage(ik_google_stream_ctx_t *sctx, yyjson_val *usage_obj)
{
    assert(sctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(usage_obj != NULL); // LCOV_EXCL_BR_LINE

    yyjson_val *prompt_tokens = yyjson_obj_get(usage_obj, "promptTokenCount"); // LCOV_EXCL_BR_LINE (key is literal)
    yyjson_val *candidates_tokens = yyjson_obj_get(usage_obj, "candidatesTokenCount"); // LCOV_EXCL_BR_LINE (key is literal)
    yyjson_val *thoughts_tokens = yyjson_obj_get(usage_obj, "thoughtsTokenCount"); // LCOV_EXCL_BR_LINE (key is literal)
    yyjson_val *total_tokens = yyjson_obj_get(usage_obj, "totalTokenCount"); // LCOV_EXCL_BR_LINE (key is literal)

    int32_t prompt = prompt_tokens ? (int32_t)yyjson_get_int(prompt_tokens) : 0;
    int32_t candidates = candidates_tokens ? (int32_t)yyjson_get_int(candidates_tokens) : 0;
    int32_t thoughts = thoughts_tokens ? (int32_t)yyjson_get_int(thoughts_tokens) : 0;

    sctx->usage.input_tokens = prompt;
    sctx->usage.thinking_tokens = thoughts;
    sctx->usage.output_tokens = candidates - thoughts; // Exclude thinking from output
    sctx->usage.total_tokens = total_tokens ? (int32_t)yyjson_get_int(total_tokens) : 0;
    sctx->usage.cached_tokens = 0; // Google doesn't report cache hits

    // End any open tool call
    ik_google_stream_end_tool_call_if_needed(sctx);

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
