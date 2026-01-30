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
 * Forward Declarations
 * ================================================================ */

static void emit_event(ik_openai_responses_stream_ctx_t *sctx, const ik_stream_event_t *event);
static void maybe_emit_start(ik_openai_responses_stream_ctx_t *sctx);
static void maybe_end_tool_call(ik_openai_responses_stream_ctx_t *sctx);

static void handle_response_created(ik_openai_responses_stream_ctx_t *sctx, yyjson_val *root);
static void handle_output_text_delta(ik_openai_responses_stream_ctx_t *sctx, yyjson_val *root);
static void handle_reasoning_summary_text_delta(ik_openai_responses_stream_ctx_t *sctx, yyjson_val *root);
static void handle_output_item_added(ik_openai_responses_stream_ctx_t *sctx, yyjson_val *root);
static void handle_function_call_arguments_delta(ik_openai_responses_stream_ctx_t *sctx, yyjson_val *root);
static void handle_output_item_done(ik_openai_responses_stream_ctx_t *sctx, yyjson_val *root);
static void handle_response_completed(ik_openai_responses_stream_ctx_t *sctx, yyjson_val *root);
static void handle_error(ik_openai_responses_stream_ctx_t *sctx, yyjson_val *root);

/* ================================================================
 * Event Emission Helpers
 * ================================================================ */

/**
 * Emit a stream event to the user callback
 */
static void emit_event(ik_openai_responses_stream_ctx_t *sctx, const ik_stream_event_t *event)
{
    DEBUG_LOG("emit_event: sctx=%p event=%p", (void *)sctx, (const void *)event);

    // Defensive NULL checks
    if (sctx == NULL) {
        DEBUG_LOG("emit_event: FATAL - sctx is NULL!");
        return;
    }

    if (event == NULL) {
        DEBUG_LOG("emit_event: FATAL - event is NULL!");
        return;
    }

    if (sctx->stream_cb == NULL) {
        DEBUG_LOG("emit_event: FATAL - stream_cb is NULL!");
        return;
    }

    DEBUG_LOG("emit_event: type=%d stream_ctx=%p", event->type, sctx->stream_ctx);
    sctx->stream_cb(event, sctx->stream_ctx);
    DEBUG_LOG("emit_event: callback returned");
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
 * Event Handlers
 * ================================================================ */

/**
 * Handle response.created event
 */
static void handle_response_created(ik_openai_responses_stream_ctx_t *sctx, yyjson_val *root)
{
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE
    assert(root != NULL); // LCOV_EXCL_BR_LINE

    yyjson_val *response_val = yyjson_obj_get(root, "response");
    if (response_val != NULL && yyjson_is_obj(response_val)) {
        yyjson_val *model_val = yyjson_obj_get(response_val, "model");
        if (model_val != NULL) {
            const char *model = yyjson_get_str_(model_val);
            if (model != NULL) {
                sctx->model = talloc_strdup(sctx, model);
                if (sctx->model == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            }
        }
    }

    maybe_emit_start(sctx);
}

/**
 * Handle response.output_text.delta event
 */
static void handle_output_text_delta(ik_openai_responses_stream_ctx_t *sctx, yyjson_val *root)
{
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE
    assert(root != NULL); // LCOV_EXCL_BR_LINE

    yyjson_val *delta_val = yyjson_obj_get(root, "delta");
    if (delta_val != NULL && yyjson_is_str(delta_val)) {
        const char *delta = yyjson_get_str_(delta_val);
        if (delta != NULL) {
            yyjson_val *content_index_val = yyjson_obj_get(root, "content_index");
            int32_t content_index = 0;
            if (content_index_val != NULL && yyjson_is_int(content_index_val)) {
                content_index = (int32_t)yyjson_get_int(content_index_val);
            }

            maybe_emit_start(sctx);

            ik_stream_event_t event = {
                .type = IK_STREAM_TEXT_DELTA,
                .index = content_index,
                .data.delta.text = delta
            };
            emit_event(sctx, &event);
        }
    }
}

/**
 * Handle response.reasoning_summary_text.delta event
 */
static void handle_reasoning_summary_text_delta(ik_openai_responses_stream_ctx_t *sctx, yyjson_val *root)
{
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE
    assert(root != NULL); // LCOV_EXCL_BR_LINE

    yyjson_val *delta_val = yyjson_obj_get(root, "delta");
    if (delta_val != NULL && yyjson_is_str(delta_val)) {
        const char *delta = yyjson_get_str_(delta_val);
        if (delta != NULL) {
            yyjson_val *summary_index_val = yyjson_obj_get(root, "summary_index");
            int32_t summary_index = 0;
            if (summary_index_val != NULL && yyjson_is_int(summary_index_val)) {
                summary_index = (int32_t)yyjson_get_int(summary_index_val);
            }

            maybe_emit_start(sctx);

            ik_stream_event_t event = {
                .type = IK_STREAM_THINKING_DELTA,
                .index = summary_index,
                .data.delta.text = delta
            };
            emit_event(sctx, &event);
        }
    }
}

/**
 * Handle response.output_item.added event
 */
static void handle_output_item_added(ik_openai_responses_stream_ctx_t *sctx, yyjson_val *root)
{
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE
    assert(root != NULL); // LCOV_EXCL_BR_LINE

    yyjson_val *item_val = yyjson_obj_get(root, "item");
    if (item_val != NULL && yyjson_is_obj(item_val)) {
        yyjson_val *type_val = yyjson_obj_get(item_val, "type");
        const char *item_type = yyjson_get_str_(type_val);

        if (item_type != NULL && strcmp(item_type, "function_call") == 0) {
            yyjson_val *output_index_val = yyjson_obj_get(root, "output_index");
            int32_t output_index = 0;
            if (output_index_val != NULL && yyjson_is_int(output_index_val)) {
                output_index = (int32_t)yyjson_get_int(output_index_val);
            }

            yyjson_val *call_id_val = yyjson_obj_get_(item_val, "call_id");
            yyjson_val *name_val = yyjson_obj_get_(item_val, "name");

            const char *call_id = yyjson_get_str_(call_id_val);
            const char *name = yyjson_get_str_(name_val);

            if (call_id != NULL && name != NULL) {
                maybe_end_tool_call(sctx);

                maybe_emit_start(sctx);

                sctx->tool_call_index = output_index;
                sctx->current_tool_id = talloc_strdup(sctx, call_id);
                if (sctx->current_tool_id == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
                sctx->current_tool_name = talloc_strdup(sctx, name);
                if (sctx->current_tool_name == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

                ik_stream_event_t event = {
                    .type = IK_STREAM_TOOL_CALL_START,
                    .index = output_index,
                    .data.tool_start.id = sctx->current_tool_id,
                    .data.tool_start.name = sctx->current_tool_name
                };
                emit_event(sctx, &event);
                sctx->in_tool_call = true;
            }
        }
    }
}

/**
 * Handle response.function_call_arguments.delta event
 */
static void handle_function_call_arguments_delta(ik_openai_responses_stream_ctx_t *sctx, yyjson_val *root)
{
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE
    assert(root != NULL); // LCOV_EXCL_BR_LINE

    yyjson_val *delta_val = yyjson_obj_get(root, "delta");
    if (delta_val != NULL && yyjson_is_str(delta_val)) {
        const char *delta = yyjson_get_str_(delta_val);
        if (delta != NULL && sctx->in_tool_call) {
            yyjson_val *output_index_val = yyjson_obj_get(root, "output_index");
            int32_t output_index = sctx->tool_call_index;
            if (output_index_val != NULL && yyjson_is_int(output_index_val)) {
                output_index = (int32_t)yyjson_get_int(output_index_val);
            }

            // Accumulate arguments
            char *new_args = talloc_asprintf(sctx, "%s%s",
                                             sctx->current_tool_args ? sctx->current_tool_args : "",
                                             delta);
            if (new_args == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            talloc_free(sctx->current_tool_args);
            sctx->current_tool_args = new_args;

            ik_stream_event_t event = {
                .type = IK_STREAM_TOOL_CALL_DELTA,
                .index = output_index,
                .data.tool_delta.arguments = delta
            };
            emit_event(sctx, &event);
        }
    }
}

/**
 * Handle response.output_item.done event
 */
static void handle_output_item_done(ik_openai_responses_stream_ctx_t *sctx, yyjson_val *root)
{
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE
    assert(root != NULL); // LCOV_EXCL_BR_LINE

    yyjson_val *output_index_val = yyjson_obj_get(root, "output_index");
    int32_t output_index = -1;
    if (output_index_val != NULL && yyjson_is_int(output_index_val)) {
        output_index = (int32_t)yyjson_get_int(output_index_val);
    }

    if (sctx->in_tool_call && output_index == sctx->tool_call_index) {
        ik_stream_event_t event = {
            .type = IK_STREAM_TOOL_CALL_DONE,
            .index = output_index
        };
        emit_event(sctx, &event);
        sctx->in_tool_call = false;

        // NOTE: Do NOT clear tool data here - response builder needs it later
    }
}

/**
 * Handle response.completed event
 */
static void handle_response_completed(ik_openai_responses_stream_ctx_t *sctx, yyjson_val *root)
{
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE
    assert(root != NULL); // LCOV_EXCL_BR_LINE

    maybe_end_tool_call(sctx);

    yyjson_val *response_val = yyjson_obj_get(root, "response");
    if (response_val != NULL && yyjson_is_obj(response_val)) {
        yyjson_val *status_val = yyjson_obj_get(response_val, "status");
        const char *status = yyjson_get_str_(status_val);

        const char *incomplete_reason = NULL;
        yyjson_val *incomplete_details_val = yyjson_obj_get_(response_val, "incomplete_details");
        if (incomplete_details_val != NULL && yyjson_is_obj(incomplete_details_val)) {
            yyjson_val *reason_val = yyjson_obj_get_(incomplete_details_val, "reason");
            incomplete_reason = yyjson_get_str_(reason_val);
        }

        if (status != NULL) {
            sctx->finish_reason = ik_openai_map_responses_status(status, incomplete_reason);
        }

        yyjson_val *usage_val = yyjson_obj_get(response_val, "usage");
        if (usage_val != NULL) {
            ik_openai_responses_parse_usage(usage_val, &sctx->usage);
        }
    }

    ik_stream_event_t event = {
        .type = IK_STREAM_DONE,
        .index = 0,
        .data.done.finish_reason = sctx->finish_reason,
        .data.done.usage = sctx->usage,
        .data.done.provider_data = NULL
    };
    emit_event(sctx, &event);
}

/**
 * Handle error event
 */
static void handle_error(ik_openai_responses_stream_ctx_t *sctx, yyjson_val *root)
{
    assert(sctx != NULL); // LCOV_EXCL_BR_LINE
    assert(root != NULL); // LCOV_EXCL_BR_LINE

    yyjson_val *error_val = yyjson_obj_get_(root, "error");
    if (error_val != NULL && yyjson_is_obj(error_val)) {
        yyjson_val *message_val = yyjson_obj_get_(error_val, "message");
        yyjson_val *type_val = yyjson_obj_get_(error_val, "type");

        const char *message = yyjson_get_str_(message_val);
        const char *type = yyjson_get_str_(type_val);

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

        ik_stream_event_t event = {
            .type = IK_STREAM_ERROR,
            .index = 0,
            .data.error.category = category,
            .data.error.message = message ? message : "Unknown error"
        };
        emit_event(sctx, &event);
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
    DEBUG_LOG("process_event: stream_ctx=%p event='%s' data_len=%zu",
              (void *)stream_ctx,
              event_name ? event_name : "(null)",
              data ? strlen(data) : 0);

    // Defensive NULL checks
    if (stream_ctx == NULL) {
        DEBUG_LOG("process_event: FATAL - stream_ctx is NULL!");
        return;
    }

    if (event_name == NULL) {
        DEBUG_LOG("process_event: FATAL - event_name is NULL!");
        return;
    }

    if (data == NULL) {
        DEBUG_LOG("process_event: FATAL - data is NULL!");
        return;
    }

    // Validate stream_ctx fields
    if (stream_ctx->stream_cb == NULL) {
        DEBUG_LOG("process_event: FATAL - stream_ctx->stream_cb is NULL!");
        return;
    }

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
        handle_response_created(stream_ctx, root);
    } else if (strcmp(event_name, "response.output_text.delta") == 0) {
        handle_output_text_delta(stream_ctx, root);
    } else if (strcmp(event_name, "response.reasoning_summary_text.delta") == 0) {
        handle_reasoning_summary_text_delta(stream_ctx, root);
    } else if (strcmp(event_name, "response.output_item.added") == 0) {
        handle_output_item_added(stream_ctx, root);
    } else if (strcmp(event_name, "response.function_call_arguments.delta") == 0) {
        handle_function_call_arguments_delta(stream_ctx, root);
    } else if (strcmp(event_name, "response.function_call_arguments.done") == 0) {
        // No-op: arguments already accumulated via delta events
        DEBUG_LOG("process_event: function_call_arguments.done - no-op");
    } else if (strcmp(event_name, "response.output_item.done") == 0) {
        handle_output_item_done(stream_ctx, root);
    } else if (strcmp(event_name, "response.completed") == 0) {
        handle_response_completed(stream_ctx, root);
    } else if (strcmp(event_name, "error") == 0) {
        handle_error(stream_ctx, root);
    } else {
        DEBUG_LOG("process_event: unknown event type '%s' - ignoring", event_name);
    }

    DEBUG_LOG("process_event: finished processing '%s'", event_name);
    yyjson_doc_free(doc);
}
