/**
 * @file request_serialize.c
 * @brief Anthropic request serialization helpers implementation
 *
 * Message and content block serialization for Anthropic's Messages API.
 */

#include "request_serialize.h"
#include "error.h"
#include "panic.h"
#include "vendor/yyjson/yyjson.h"
#include <string.h>
#include <assert.h>

/**
 * Serialize a single content block to Anthropic JSON format
 */
bool ik_anthropic_serialize_content_block(yyjson_mut_doc *doc, yyjson_mut_val *arr,
                                          const ik_content_block_t *block)
{
    assert(doc != NULL);   // LCOV_EXCL_BR_LINE
    assert(arr != NULL);   // LCOV_EXCL_BR_LINE
    assert(block != NULL); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    if (!obj) return false; // LCOV_EXCL_BR_LINE

    switch (block->type) {
        case IK_CONTENT_TEXT:
            if (!yyjson_mut_obj_add_str(doc, obj, "type", "text")) return false; // LCOV_EXCL_BR_LINE
            if (!yyjson_mut_obj_add_str(doc, obj, "text", block->data.text.text)) return false; // LCOV_EXCL_BR_LINE
            break;

        case IK_CONTENT_THINKING:
            if (!yyjson_mut_obj_add_str(doc, obj, "type", "thinking")) return false; // LCOV_EXCL_BR_LINE
            if (!yyjson_mut_obj_add_str(doc, obj, "thinking", block->data.thinking.text)) return false; // LCOV_EXCL_BR_LINE
            break;

        case IK_CONTENT_TOOL_CALL:
            if (!yyjson_mut_obj_add_str(doc, obj, "type", "tool_use")) return false; // LCOV_EXCL_BR_LINE
            if (!yyjson_mut_obj_add_str(doc, obj, "id", block->data.tool_call.id)) return false; // LCOV_EXCL_BR_LINE
            if (!yyjson_mut_obj_add_str(doc, obj, "name", block->data.tool_call.name)) return false; // LCOV_EXCL_BR_LINE

            // Parse arguments JSON string and add as object
            yyjson_doc *args_doc = yyjson_read(block->data.tool_call.arguments,
                                               strlen(block->data.tool_call.arguments), 0);
            if (!args_doc) return false; // LCOV_EXCL_BR_LINE

            yyjson_mut_val *args_mut = yyjson_val_mut_copy(doc, yyjson_doc_get_root(args_doc));
            yyjson_doc_free(args_doc);
            if (!args_mut) return false; // LCOV_EXCL_BR_LINE

            if (!yyjson_mut_obj_add_val(doc, obj, "input", args_mut)) return false; // LCOV_EXCL_BR_LINE
            break;

        case IK_CONTENT_TOOL_RESULT:
            if (!yyjson_mut_obj_add_str(doc, obj, "type", "tool_result")) return false; // LCOV_EXCL_BR_LINE
            if (!yyjson_mut_obj_add_str(doc, obj, "tool_use_id", block->data.tool_result.tool_call_id)) return false; // LCOV_EXCL_BR_LINE
            if (!yyjson_mut_obj_add_str(doc, obj, "content", block->data.tool_result.content)) return false; // LCOV_EXCL_BR_LINE
            if (!yyjson_mut_obj_add_bool(doc, obj, "is_error", block->data.tool_result.is_error)) return false; // LCOV_EXCL_BR_LINE
            break;

        default: // LCOV_EXCL_LINE
            return false; // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_arr_add_val(arr, obj)) return false; // LCOV_EXCL_BR_LINE
    return true;
}

/**
 * Serialize message content
 *
 * If single text block, use string format.
 * Otherwise, use array format.
 */
bool ik_anthropic_serialize_message_content(yyjson_mut_doc *doc, yyjson_mut_val *msg_obj,
                                            const ik_message_t *message)
{
    assert(doc != NULL);     // LCOV_EXCL_BR_LINE
    assert(msg_obj != NULL); // LCOV_EXCL_BR_LINE
    assert(message != NULL); // LCOV_EXCL_BR_LINE

    // Single text block uses simple string format
    if (message->content_count == 1 && message->content_blocks[0].type == IK_CONTENT_TEXT) {
        if (!yyjson_mut_obj_add_str(doc, msg_obj, "content",
                                     message->content_blocks[0].data.text.text)) {
            return false; // LCOV_EXCL_BR_LINE
        }
        return true;
    }

    // Multiple blocks or non-text blocks use array format
    yyjson_mut_val *content_arr = yyjson_mut_arr(doc);
    if (!content_arr) return false; // LCOV_EXCL_BR_LINE

    for (size_t i = 0; i < message->content_count; i++) {
        if (!ik_anthropic_serialize_content_block(doc, content_arr, &message->content_blocks[i])) {
            return false;
        }
    }

    if (!yyjson_mut_obj_add_val(doc, msg_obj, "content", content_arr)) {
        return false; // LCOV_EXCL_BR_LINE
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

    yyjson_mut_val *messages_arr = yyjson_mut_arr(doc);
    if (!messages_arr) return false; // LCOV_EXCL_BR_LINE

    for (size_t i = 0; i < req->message_count; i++) {
        yyjson_mut_val *msg_obj = yyjson_mut_obj(doc);
        if (!msg_obj) return false; // LCOV_EXCL_BR_LINE

        // Add role
        const char *role_str = ik_anthropic_role_to_string(req->messages[i].role);
        if (!yyjson_mut_obj_add_str(doc, msg_obj, "role", role_str)) {
            return false; // LCOV_EXCL_BR_LINE
        }

        // Add content
        if (!ik_anthropic_serialize_message_content(doc, msg_obj, &req->messages[i])) {
            return false;
        }

        if (!yyjson_mut_arr_add_val(messages_arr, msg_obj)) {
            return false; // LCOV_EXCL_BR_LINE
        }
    }

    if (!yyjson_mut_obj_add_val(doc, root, "messages", messages_arr)) {
        return false; // LCOV_EXCL_BR_LINE
    }
    return true;
}
