/**
 * @file streaming_events.c
 * @brief Anthropic SSE event processors
 */

#include "streaming_events.h"
#include "response.h"
#include "panic.h"
#include <assert.h>
#include <string.h>

/**
 * Process message_start event
 */
void ik_anthropic_process_message_start(ik_anthropic_stream_ctx_t *sctx, yyjson_val *root)
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
void ik_anthropic_process_content_block_start(ik_anthropic_stream_ctx_t *sctx, yyjson_val *root)
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
        sctx->current_block_type = IK_CONTENT_TOOL_CALL; // LCOV_EXCL_BR_LINE

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
void ik_anthropic_process_content_block_delta(ik_anthropic_stream_ctx_t *sctx, yyjson_val *root)
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
                // Accumulate arguments (like OpenAI does)
                char *new_args = talloc_asprintf(sctx, "%s%s",
                    sctx->current_tool_args ? sctx->current_tool_args : "",
                    partial_json);
                if (new_args == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
                talloc_free(sctx->current_tool_args);
                sctx->current_tool_args = new_args;

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
void ik_anthropic_process_content_block_stop(ik_anthropic_stream_ctx_t *sctx, yyjson_val *root)
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

        // NOTE: Do NOT clear tool data here - response builder needs it later
    }

    // Reset current block tracking
    sctx->current_block_index = -1;
}

/**
 * Process message_delta event
 */
void ik_anthropic_process_message_delta(ik_anthropic_stream_ctx_t *sctx, yyjson_val *root)
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
void ik_anthropic_process_message_stop(ik_anthropic_stream_ctx_t *sctx, yyjson_val *root)
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
void ik_anthropic_process_error(ik_anthropic_stream_ctx_t *sctx, yyjson_val *root)
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
    const char *type_str = NULL; // LCOV_EXCL_BR_LINE
    yyjson_val *type_val = yyjson_obj_get(error_obj, "type");
    if (type_val != NULL) {
        type_str = yyjson_get_str(type_val);
    }

    // Extract error message
    const char *message = "Unknown error"; // LCOV_EXCL_BR_LINE
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
