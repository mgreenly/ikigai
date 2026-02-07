/**
 * @file request_serialize.c
 * @brief Anthropic request serialization helpers implementation
 *
 * Message and content block serialization for Anthropic's Messages API.
 */

#include "apps/ikigai/providers/anthropic/request_serialize.h"

#include "apps/ikigai/debug_log.h"
#include "apps/ikigai/providers/anthropic/error.h"
#include "shared/panic.h"
#include "shared/wrapper_json.h"

#include <assert.h>
#include <string.h>


#include "shared/poison.h"
/**
 * Serialize a single content block to Anthropic JSON format
 */
bool ik_anthropic_serialize_content_block(yyjson_mut_doc *doc, yyjson_mut_val *arr,
                                          const ik_content_block_t *block,
                                          size_t message_idx, size_t block_idx)
{
    assert(doc != NULL);   // LCOV_EXCL_BR_LINE
    assert(arr != NULL);   // LCOV_EXCL_BR_LINE
    assert(block != NULL); // LCOV_EXCL_BR_LINE

    DEBUG_LOG("serialize_content_block: msg[%zu] block[%zu] type=%d", message_idx, block_idx, block->type);

    yyjson_mut_val *obj = yyjson_mut_obj_(doc);
    if (!obj) {
        DEBUG_LOG("serialize_content_block: msg[%zu] block[%zu] - failed to create JSON object", message_idx, block_idx);
        return false;
    }

    switch (block->type) {
        case IK_CONTENT_TEXT:
            if (!yyjson_mut_obj_add_str_(doc, obj, "type", "text")) {
                DEBUG_LOG("serialize_content_block: msg[%zu] block[%zu] TEXT - failed to add type field", message_idx, block_idx);
                return false;
            }
            if (!yyjson_mut_obj_add_str_(doc, obj, "text", block->data.text.text)) {
                DEBUG_LOG("serialize_content_block: msg[%zu] block[%zu] TEXT - failed to add text field (text=%s)",
                          message_idx, block_idx, block->data.text.text ? "(non-NULL)" : "(NULL)");
                return false;
            }
            break;

        case IK_CONTENT_THINKING:
            if (!yyjson_mut_obj_add_str_(doc, obj, "type", "thinking")) {
                DEBUG_LOG("serialize_content_block: msg[%zu] block[%zu] THINKING - failed to add type field", message_idx, block_idx);
                return false;
            }
            DEBUG_LOG("serialize_content_block: msg[%zu] block[%zu] THINKING - text=%s signature=%s",
                      message_idx, block_idx,
                      block->data.thinking.text ? "(non-NULL)" : "(NULL)",
                      block->data.thinking.signature ? "(non-NULL)" : "(NULL)");
            // Thinking text must not be NULL or empty
            if (block->data.thinking.text == NULL || block->data.thinking.text[0] == '\0') {
                DEBUG_LOG("serialize_content_block: msg[%zu] block[%zu] THINKING - thinking text is NULL or empty",
                          message_idx, block_idx);
                return false;
            }
            if (!yyjson_mut_obj_add_str_(doc, obj, "thinking", block->data.thinking.text)) {
                DEBUG_LOG("serialize_content_block: msg[%zu] block[%zu] THINKING - failed to add thinking field (text=%s)",
                          message_idx, block_idx, block->data.thinking.text ? "(non-NULL)" : "(NULL)");
                return false;
            }
            if (block->data.thinking.signature != NULL) {
                if (!yyjson_mut_obj_add_str_(doc, obj, "signature", block->data.thinking.signature)) {
                    DEBUG_LOG("serialize_content_block: msg[%zu] block[%zu] THINKING - failed to add signature field", message_idx, block_idx);
                    return false;
                }
            }
            break;

        case IK_CONTENT_TOOL_CALL:
            if (!yyjson_mut_obj_add_str_(doc, obj, "type", "tool_use")) {
                DEBUG_LOG("serialize_content_block: msg[%zu] block[%zu] TOOL_CALL - failed to add type field", message_idx, block_idx);
                return false;
            }
            if (!yyjson_mut_obj_add_str_(doc, obj, "id", block->data.tool_call.id)) {
                DEBUG_LOG("serialize_content_block: msg[%zu] block[%zu] TOOL_CALL - failed to add id field", message_idx, block_idx);
                return false;
            }
            if (!yyjson_mut_obj_add_str_(doc, obj, "name", block->data.tool_call.name)) {
                DEBUG_LOG("serialize_content_block: msg[%zu] block[%zu] TOOL_CALL - failed to add name field", message_idx, block_idx);
                return false;
            }

            // Parse arguments JSON string and add as object
            // If arguments is empty string, treat as empty object "{}"
            const char *args_str = block->data.tool_call.arguments;
            if (args_str == NULL || args_str[0] == '\0') {
                args_str = "{}";
            }
            DEBUG_LOG("serialize_content_block: msg[%zu] block[%zu] TOOL_CALL - arguments=%s",
                      message_idx, block_idx, args_str);
            yyjson_doc *args_doc = yyjson_read(args_str, strlen(args_str), 0);
            if (!args_doc) {
                DEBUG_LOG("serialize_content_block: msg[%zu] block[%zu] TOOL_CALL - failed to parse arguments JSON: '%s'",
                          message_idx, block_idx, args_str);
                return false;
            }

            yyjson_mut_val *args_mut = yyjson_val_mut_copy_(doc, yyjson_doc_get_root(args_doc));
            yyjson_doc_free(args_doc);
            if (!args_mut) {
                DEBUG_LOG("serialize_content_block: msg[%zu] block[%zu] TOOL_CALL - failed to copy arguments to mutable JSON", message_idx, block_idx);
                return false;
            }

            if (!yyjson_mut_obj_add_val_(doc, obj, "input", args_mut)) {
                DEBUG_LOG("serialize_content_block: msg[%zu] block[%zu] TOOL_CALL - failed to add input field", message_idx, block_idx);
                return false;
            }
            break;

        case IK_CONTENT_TOOL_RESULT:
            if (!yyjson_mut_obj_add_str_(doc, obj, "type", "tool_result")) {
                DEBUG_LOG("serialize_content_block: msg[%zu] block[%zu] TOOL_RESULT - failed to add type field", message_idx, block_idx);
                return false;
            }
            if (!yyjson_mut_obj_add_str_(doc, obj, "tool_use_id", block->data.tool_result.tool_call_id)) {
                DEBUG_LOG("serialize_content_block: msg[%zu] block[%zu] TOOL_RESULT - failed to add tool_use_id field", message_idx, block_idx);
                return false;
            }
            if (!yyjson_mut_obj_add_str_(doc, obj, "content", block->data.tool_result.content)) {
                DEBUG_LOG("serialize_content_block: msg[%zu] block[%zu] TOOL_RESULT - failed to add content field", message_idx, block_idx);
                return false;
            }
            if (!yyjson_mut_obj_add_bool_(doc, obj, "is_error", block->data.tool_result.is_error)) {
                DEBUG_LOG("serialize_content_block: msg[%zu] block[%zu] TOOL_RESULT - failed to add is_error field", message_idx, block_idx);
                return false;
            }
            break;

        case IK_CONTENT_REDACTED_THINKING:
            if (!yyjson_mut_obj_add_str_(doc, obj, "type", "redacted_thinking")) {
                DEBUG_LOG("serialize_content_block: msg[%zu] block[%zu] REDACTED_THINKING - failed to add type field", message_idx, block_idx);
                return false;
            }
            DEBUG_LOG("serialize_content_block: msg[%zu] block[%zu] REDACTED_THINKING - data=%s",
                      message_idx, block_idx,
                      block->data.redacted_thinking.data ? "(non-NULL)" : "(NULL)");
            // Redacted thinking data must not be NULL or empty
            if (block->data.redacted_thinking.data == NULL || block->data.redacted_thinking.data[0] == '\0') {
                DEBUG_LOG("serialize_content_block: msg[%zu] block[%zu] REDACTED_THINKING - redacted data is NULL or empty",
                          message_idx, block_idx);
                return false;
            }
            if (!yyjson_mut_obj_add_str_(doc, obj, "data", block->data.redacted_thinking.data)) {
                DEBUG_LOG("serialize_content_block: msg[%zu] block[%zu] REDACTED_THINKING - failed to add data field (data=%s)",
                          message_idx, block_idx, block->data.redacted_thinking.data ? "(non-NULL)" : "(NULL)");
                return false;
            }
            break;

        default: // LCOV_EXCL_LINE
            DEBUG_LOG("serialize_content_block: msg[%zu] block[%zu] - unknown block type %d", message_idx, block_idx, block->type); // LCOV_EXCL_LINE
            return false; // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_arr_add_val_(arr, obj)) {
        DEBUG_LOG("serialize_content_block: msg[%zu] block[%zu] - failed to add block to array", message_idx, block_idx);
        return false;
    }
    return true;
}

/**
 * Serialize message content
 *
 * If single text block, use string format.
 * Otherwise, use array format.
 */
bool ik_anthropic_serialize_message_content(yyjson_mut_doc *doc, yyjson_mut_val *msg_obj,
                                            const ik_message_t *message, size_t message_idx)
{
    assert(doc != NULL);     // LCOV_EXCL_BR_LINE
    assert(msg_obj != NULL); // LCOV_EXCL_BR_LINE
    assert(message != NULL); // LCOV_EXCL_BR_LINE

    DEBUG_LOG("serialize_message_content: msg[%zu] content_count=%zu", message_idx, message->content_count);

    // Single text block uses simple string format
    if (message->content_count == 1 && message->content_blocks[0].type == IK_CONTENT_TEXT) {
        if (!yyjson_mut_obj_add_str_(doc, msg_obj, "content",
                                     message->content_blocks[0].data.text.text)) {
            DEBUG_LOG("serialize_message_content: msg[%zu] - failed to add single text content", message_idx);
            return false;
        }
        return true;
    }

    // Multiple blocks or non-text blocks use array format
    yyjson_mut_val *content_arr = yyjson_mut_arr_(doc);
    if (!content_arr) {
        DEBUG_LOG("serialize_message_content: msg[%zu] - failed to create content array", message_idx);
        return false;
    }

    for (size_t i = 0; i < message->content_count; i++) {
        if (!ik_anthropic_serialize_content_block(doc, content_arr, &message->content_blocks[i], message_idx, i)) {
            return false;
        }
    }

    if (!yyjson_mut_obj_add_val_(doc, msg_obj, "content", content_arr)) {
        DEBUG_LOG("serialize_message_content: msg[%zu] - failed to add content array to message", message_idx);
        return false;
    }
    return true;
}

/**
 * Map internal role to Anthropic role string
 */
const char *ik_anthropic_role_to_string(ik_role_t role)
{
    switch (role) {
        case IK_ROLE_USER:
            return "user";
        case IK_ROLE_ASSISTANT:
            return "assistant";
        case IK_ROLE_TOOL:
            return "user"; // Tool results are sent as user messages in Anthropic
        default: // LCOV_EXCL_LINE
            return "user"; // LCOV_EXCL_LINE
    }
}

/**
 * Serialize messages array
 */
bool ik_anthropic_serialize_messages(yyjson_mut_doc *doc, yyjson_mut_val *root,
                                     const ik_request_t *req)
{
    assert(doc != NULL);  // LCOV_EXCL_BR_LINE
    assert(root != NULL); // LCOV_EXCL_BR_LINE
    assert(req != NULL);  // LCOV_EXCL_BR_LINE

    DEBUG_LOG("serialize_messages: ENTRY message_count=%zu", req->message_count);

    yyjson_mut_val *messages_arr = yyjson_mut_arr_(doc);
    if (!messages_arr) {
        DEBUG_LOG("serialize_messages: failed to create messages array");
        return false;
    }

    for (size_t i = 0; i < req->message_count; i++) {
        DEBUG_LOG("serialize_messages: msg[%zu] role=%d", i, req->messages[i].role);

        yyjson_mut_val *msg_obj = yyjson_mut_obj_(doc);
        if (!msg_obj) {
            DEBUG_LOG("serialize_messages: msg[%zu] - failed to create message object", i);
            return false;
        }

        // Add role
        const char *role_str = ik_anthropic_role_to_string(req->messages[i].role);
        if (!yyjson_mut_obj_add_str_(doc, msg_obj, "role", role_str)) {
            DEBUG_LOG("serialize_messages: msg[%zu] - failed to add role field", i);
            return false;
        }

        // Add content
        if (!ik_anthropic_serialize_message_content(doc, msg_obj, &req->messages[i], i)) {
            DEBUG_LOG("serialize_messages: msg[%zu] - failed to serialize message content", i);
            return false;
        }

        if (!yyjson_mut_arr_add_val_(messages_arr, msg_obj)) {
            DEBUG_LOG("serialize_messages: msg[%zu] - failed to add message to array", i);
            return false;
        }
    }

    if (!yyjson_mut_obj_add_val_(doc, root, "messages", messages_arr)) {
        DEBUG_LOG("serialize_messages: failed to add messages array to root");
        return false;
    }

    DEBUG_LOG("serialize_messages: EXIT success");
    return true;
}
