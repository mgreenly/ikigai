/**
 * @file streaming_chat_delta.c
 * @brief OpenAI Chat Completions delta processing
 */

#include "apps/ikigai/providers/openai/streaming_chat_internal.h"
#include "apps/ikigai/providers/openai/response.h"
#include "shared/panic.h"
#include "vendor/yyjson/yyjson.h"
#include <assert.h>
#include <string.h>


#include "shared/poison.h"
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
 *
 * NOTE: Chat completions API does not use tool calls - this path is only
 * exercised by the responses API. Coverage exclusion is appropriate here.
 */
void ik_openai_chat_maybe_end_tool_call(ik_openai_chat_stream_ctx_t *sctx)
{
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE

    if (sctx->in_tool_call) { // LCOV_EXCL_BR_LINE - chat completions never sets in_tool_call
        ik_stream_event_t event = { // LCOV_EXCL_LINE - responses API only
            .type = IK_STREAM_TOOL_CALL_DONE, // LCOV_EXCL_LINE
            .index = sctx->tool_call_index // LCOV_EXCL_LINE
        }; // LCOV_EXCL_LINE
        emit_event(sctx, &event); // LCOV_EXCL_LINE
        sctx->in_tool_call = false; // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE
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
 * Process choices[0].delta object
 */
void ik_openai_chat_process_delta(ik_openai_chat_stream_ctx_t *sctx, void *delta_ptr, const char *finish_reason_str)
{
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE
    assert(delta_ptr != NULL); // LCOV_EXCL_BR_LINE

    yyjson_val *delta = (yyjson_val *)delta_ptr; // LCOV_EXCL_BR_LINE - cast always succeeds

    process_content_delta(sctx, delta);

    if (finish_reason_str != NULL) {
        sctx->finish_reason = ik_openai_map_chat_finish_reason(finish_reason_str);
    }
}
