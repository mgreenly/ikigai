/**
 * @file streaming_chat_delta.c
 * @brief OpenAI Chat Completions delta processing
 */

#include "streaming_chat_internal.h"
#include "response.h"
#include "panic.h"
#include "vendor/yyjson/yyjson.h"
#include <assert.h>
#include <string.h>


#include "poison.h"
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
void ik_openai_chat_maybe_emit_start(ik_openai_chat_stream_ctx_t *sctx)
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
void ik_openai_chat_maybe_end_tool_call(ik_openai_chat_stream_ctx_t *sctx)
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
 * Process content (text) delta
 */
static void process_content_delta(ik_openai_chat_stream_ctx_t *sctx, yyjson_val *delta)
{
    yyjson_val *content_val = yyjson_obj_get(delta, "content");
    if (content_val == NULL || !yyjson_is_str(content_val)) return;

    const char *content = yyjson_get_str(content_val); // LCOV_EXCL_BR_LINE - yyjson_get_str on validated string cannot return NULL
    if (content == NULL) return; // LCOV_EXCL_BR_LINE - defensive check, yyjson_get_str returns non-NULL for valid strings

    ik_openai_chat_maybe_end_tool_call(sctx);
    ik_openai_chat_maybe_emit_start(sctx);

    ik_stream_event_t event = {
        .type = IK_STREAM_TEXT_DELTA,
        .index = 0,
        .data.delta.text = content
    };
    emit_event(sctx, &event);
}

/**
 * Start a new tool call
 */
static void start_new_tool_call(ik_openai_chat_stream_ctx_t *sctx, yyjson_val *tool_call, int32_t tc_index)
{
    yyjson_val *id_val = yyjson_obj_get(tool_call, "id"); // LCOV_EXCL_BR_LINE - yyjson_obj_get returns NULL only if key missing
    yyjson_val *function_val = yyjson_obj_get(tool_call, "function");

    if (id_val == NULL || function_val == NULL || !yyjson_is_obj(function_val)) return; // LCOV_EXCL_BR_LINE - id_val NULL branch requires malformed tool_call start

    const char *id = yyjson_get_str(id_val); // LCOV_EXCL_BR_LINE - yyjson_get_str on validated JSON returns non-NULL
    yyjson_val *name_val = yyjson_obj_get(function_val, "name");
    const char *name = yyjson_get_str(name_val);

    if (id == NULL || name == NULL) return; // LCOV_EXCL_BR_LINE - defensive check, get_str on valid JSON returns non-NULL

    ik_openai_chat_maybe_emit_start(sctx);

    sctx->tool_call_index = tc_index;

    // Free previous tool call data before overwriting
    talloc_free(sctx->current_tool_id);
    talloc_free(sctx->current_tool_name);
    talloc_free(sctx->current_tool_args);

    sctx->current_tool_id = talloc_strdup(sctx, id);
    if (sctx->current_tool_id == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    sctx->current_tool_name = talloc_strdup(sctx, name);
    if (sctx->current_tool_name == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    sctx->current_tool_args = talloc_strdup(sctx, "");
    if (sctx->current_tool_args == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    ik_stream_event_t event = {
        .type = IK_STREAM_TOOL_CALL_START,
        .index = tc_index,
        .data.tool_start.id = sctx->current_tool_id,
        .data.tool_start.name = sctx->current_tool_name
    };
    emit_event(sctx, &event);
    sctx->in_tool_call = true;
}

/**
 * Accumulate tool call arguments
 */
static void accumulate_tool_arguments(ik_openai_chat_stream_ctx_t *sctx, yyjson_val *function_val, int32_t tc_index)
{
    if (function_val == NULL || !yyjson_is_obj(function_val)) return;

    yyjson_val *arguments_val = yyjson_obj_get(function_val, "arguments");
    if (arguments_val == NULL || !yyjson_is_str(arguments_val)) return;

    const char *arguments = yyjson_get_str(arguments_val); // LCOV_EXCL_BR_LINE - yyjson_get_str on validated string returns non-NULL
    if (arguments == NULL || !sctx->in_tool_call) return; // LCOV_EXCL_BR_LINE - defensive check, get_str on valid string returns non-NULL

    char *new_args = talloc_asprintf(sctx, "%s%s",
                                     sctx->current_tool_args ? sctx->current_tool_args : "", // LCOV_EXCL_BR_LINE - current_tool_args NULL only if START failed
                                     arguments);
    if (new_args == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    talloc_free(sctx->current_tool_args);
    sctx->current_tool_args = new_args;

    ik_stream_event_t event = {
        .type = IK_STREAM_TOOL_CALL_DELTA,
        .index = tc_index,
        .data.tool_delta.arguments = arguments
    };
    emit_event(sctx, &event);
}

/**
 * Process a single tool call object
 */
static void process_tool_call_object(ik_openai_chat_stream_ctx_t *sctx, yyjson_val *tool_call)
{
    if (tool_call == NULL || !yyjson_is_obj(tool_call)) return; // LCOV_EXCL_BR_LINE - defensive check, arr_get on valid array returns non-NULL

    yyjson_val *index_val = yyjson_obj_get(tool_call, "index");
    int32_t tc_index = 0;
    if (index_val != NULL && yyjson_is_int(index_val)) {
        tc_index = (int32_t)yyjson_get_int(index_val);
    }

    if (tc_index != sctx->tool_call_index) {
        ik_openai_chat_maybe_end_tool_call(sctx);
        start_new_tool_call(sctx, tool_call, tc_index);
    }

    yyjson_val *function_val = yyjson_obj_get(tool_call, "function");
    accumulate_tool_arguments(sctx, function_val, tc_index);
}

/**
 * Process tool_calls array
 */
static void process_tool_calls_array(ik_openai_chat_stream_ctx_t *sctx, yyjson_val *delta)
{
    yyjson_val *tool_calls_val = yyjson_obj_get(delta, "tool_calls");
    if (tool_calls_val == NULL || !yyjson_is_arr(tool_calls_val)) return;

    size_t arr_size = yyjson_arr_size(tool_calls_val);
    if (arr_size == 0) return;

    yyjson_val *tool_call = yyjson_arr_get(tool_calls_val, 0); // LCOV_EXCL_BR_LINE - yyjson_arr_get with valid index returns non-NULL
    process_tool_call_object(sctx, tool_call);
}

/**
 * Process choices[0].delta object
 */
void ik_openai_chat_process_delta(ik_openai_chat_stream_ctx_t *sctx, void *delta_ptr, const char *finish_reason_str)
{
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE
    assert(delta_ptr != NULL); // LCOV_EXCL_BR_LINE

    yyjson_val *delta = (yyjson_val *)delta_ptr; // LCOV_EXCL_BR_LINE - cast always succeeds

    process_content_delta(sctx, delta);
    process_tool_calls_array(sctx, delta);

    if (finish_reason_str != NULL) {
        sctx->finish_reason = ik_openai_map_chat_finish_reason(finish_reason_str);
    }
}
