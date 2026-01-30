/**
 * @file streaming_responses_events.c
 * @brief OpenAI Responses API event processing
 */

#include "streaming_responses_internal.h"

#include "debug_log.h"
#include "error.h"
#include "panic.h"
#include "response.h"
#include "wrapper_json.h"

#include <assert.h>
#include <string.h>

/* ================================================================
 * Event Emission Helpers
 * ================================================================ */

/**
 * Emit a stream event to the user callback
 */
void ik_openai_emit_event(ik_openai_responses_stream_ctx_t *sctx, const ik_stream_event_t *event)
{
    assert(sctx != NULL);           // LCOV_EXCL_BR_LINE
    assert(event != NULL);          // LCOV_EXCL_BR_LINE
    assert(sctx->stream_cb != NULL); // LCOV_EXCL_BR_LINE

    DEBUG_LOG("emit_event: type=%d stream_ctx=%p", event->type, sctx->stream_ctx);
    sctx->stream_cb(event, sctx->stream_ctx);
}

/**
 * Emit IK_STREAM_START if not yet started
 */
void ik_openai_maybe_emit_start(ik_openai_responses_stream_ctx_t *sctx)
{
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE

    if (!sctx->started) {
        ik_stream_event_t event = {
            .type = IK_STREAM_START,
            .index = 0,
            .data.start.model = sctx->model
        };
        ik_openai_emit_event(sctx, &event);
        sctx->started = true;
    }
}

/**
 * Emit IK_STREAM_TOOL_CALL_DONE if in a tool call
 */
void ik_openai_maybe_end_tool_call(ik_openai_responses_stream_ctx_t *sctx)
{
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE

    if (sctx->in_tool_call) {
        ik_stream_event_t event = {
            .type = IK_STREAM_TOOL_CALL_DONE,
            .index = sctx->tool_call_index
        };
        ik_openai_emit_event(sctx, &event);
        sctx->in_tool_call = false;
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
    assert(stream_ctx != NULL);           // LCOV_EXCL_BR_LINE
    assert(event_name != NULL);           // LCOV_EXCL_BR_LINE
    assert(data != NULL);                 // LCOV_EXCL_BR_LINE
    assert(stream_ctx->stream_cb != NULL); // LCOV_EXCL_BR_LINE

    DEBUG_LOG("process_event: event='%s' data_len=%zu", event_name, strlen(data));

    yyjson_doc *doc = yyjson_read(data, strlen(data), 0);
    if (doc == NULL) {
        DEBUG_LOG("process_event: yyjson_read failed for event '%s'", event_name);
        return;
    }

    yyjson_val *root = yyjson_doc_get_root_(doc);
    if (root == NULL || !yyjson_is_obj(root)) {
        DEBUG_LOG("process_event: invalid JSON root for event '%s'", event_name);
        yyjson_doc_free(doc);
        return;
    }

    DEBUG_LOG("process_event: dispatching event '%s'", event_name);

    if (strcmp(event_name, "response.created") == 0) {
        ik_openai_responses_handle_response_created(stream_ctx, root);
    } else if (strcmp(event_name, "response.output_text.delta") == 0) {
        ik_openai_responses_handle_output_text_delta(stream_ctx, root);
    } else if (strcmp(event_name, "response.reasoning_summary_text.delta") == 0) {
        ik_openai_responses_handle_reasoning_summary_text_delta(stream_ctx, root);
    } else if (strcmp(event_name, "response.output_item.added") == 0) {
        ik_openai_responses_handle_output_item_added(stream_ctx, root);
    } else if (strcmp(event_name, "response.function_call_arguments.delta") == 0) {
        ik_openai_responses_handle_function_call_arguments_delta(stream_ctx, root);
    } else if (strcmp(event_name, "response.function_call_arguments.done") == 0) {
        // No-op: arguments already accumulated via delta events
        DEBUG_LOG("process_event: function_call_arguments.done - no-op");
    } else if (strcmp(event_name, "response.output_item.done") == 0) {
        ik_openai_responses_handle_output_item_done(stream_ctx, root);
    } else if (strcmp(event_name, "response.completed") == 0) {
        ik_openai_responses_handle_response_completed(stream_ctx, root);
    } else if (strcmp(event_name, "error") == 0) {
        ik_openai_responses_handle_error_event(stream_ctx, root);
    } else {
        DEBUG_LOG("process_event: unknown event type '%s' - ignoring", event_name);
    }

    DEBUG_LOG("process_event: finished processing '%s'", event_name);
    yyjson_doc_free(doc);
}
