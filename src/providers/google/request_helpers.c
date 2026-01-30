/**
 * @file request_helpers.c
 * @brief Google request serialization helper functions
 *
 * Helper functions for transforming content blocks and extracting metadata
 * from the canonical request format to Google Gemini's native API format.
 */

#include "request_helpers.h"
#include "thinking.h"
#include "error.h"
#include "panic.h"
#include "vendor/yyjson/yyjson.h"
#include "wrapper_json.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>

/* ================================================================
 * Helper Functions
 * ================================================================ */

/**
 * Map internal role to Google role string
 */
const char *ik_google_role_to_string(ik_role_t role, const char *model)
{
    switch (role) {
        case IK_ROLE_USER:
            return "user";
        case IK_ROLE_ASSISTANT:
            return "model";
        case IK_ROLE_TOOL:
            // Gemini 3 requires "user" role for tool results
            if (model != NULL && ik_google_model_series(model) == IK_GEMINI_3) {
                return "user";
            }
            return "function";
        default: // LCOV_EXCL_LINE
            return "user"; // LCOV_EXCL_LINE
    }
}

/**
 * Find function name for a tool_call_id by scanning previous messages
 */
static const char *find_function_name_for_tool_call(const ik_message_t *messages,
                                                     size_t message_count,
                                                     size_t current_idx,
                                                     const char *tool_call_id)
{
    // Scan backwards from current message to find matching tool call
    for (size_t i = 0; i < current_idx && i < message_count; i++) {
        const ik_message_t *msg = &messages[i];
        for (size_t j = 0; j < msg->content_count; j++) {
            if (msg->content_blocks[j].type == IK_CONTENT_TOOL_CALL) {
                if (strcmp(msg->content_blocks[j].data.tool_call.id, tool_call_id) == 0) {
                    return msg->content_blocks[j].data.tool_call.name;
                }
            }
        }
    }
    return NULL;  // Not found
}

/**
 * Serialize a single content block to Google JSON format
 */
bool ik_google_serialize_content_block(yyjson_mut_doc *doc, yyjson_mut_val *arr,
                                       const ik_content_block_t *block, const char *model,
                                       const ik_message_t *messages, size_t message_count,
                                       size_t current_msg_idx)
{
    assert(doc != NULL);   // LCOV_EXCL_BR_LINE
    assert(arr != NULL);   // LCOV_EXCL_BR_LINE
    assert(block != NULL); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    if (!obj) return false; // LCOV_EXCL_BR_LINE

    switch (block->type) { // LCOV_EXCL_BR_LINE
        case IK_CONTENT_TEXT:
            if (!yyjson_mut_obj_add_str_(doc, obj, "text", block->data.text.text)) {
                return false;
            }
            break;

        case IK_CONTENT_THINKING:
            if (!yyjson_mut_obj_add_str_(doc, obj, "text", block->data.thinking.text)) {
                return false;
            }
            if (!yyjson_mut_obj_add_bool_(doc, obj, "thought", true)) {
                return false;
            }
            break;

        case IK_CONTENT_TOOL_CALL: { // LCOV_EXCL_BR_LINE
            // Add thought signature if present (Gemini 3 only)
            if (block->data.tool_call.thought_signature != NULL &&
                model != NULL && ik_google_model_series(model) == IK_GEMINI_3) {
                if (!yyjson_mut_obj_add_str_(doc, obj, "thoughtSignature",
                                             block->data.tool_call.thought_signature)) {
                    return false;
                }
            }

            // Build functionCall object
            yyjson_mut_val *func_obj = yyjson_mut_obj(doc);
            if (!func_obj) return false; // LCOV_EXCL_BR_LINE

            if (!yyjson_mut_obj_add_str_(doc, func_obj, "name", block->data.tool_call.name)) {
                return false;
            }

            // Parse arguments JSON string and add as object
            yyjson_doc *args_doc = yyjson_read(block->data.tool_call.arguments,
                                               strlen(block->data.tool_call.arguments), 0);
            if (!args_doc) return false; // LCOV_EXCL_BR_LINE

            yyjson_mut_val *args_mut = yyjson_val_mut_copy_(doc, yyjson_doc_get_root(args_doc));
            yyjson_doc_free(args_doc);
            if (!args_mut) return false;

            if (!yyjson_mut_obj_add_val_(doc, func_obj, "args", args_mut)) {
                return false;
            }

            if (!yyjson_mut_obj_add_val_(doc, obj, "functionCall", func_obj)) {
                return false; // LCOV_EXCL_BR_LINE
            }
            break;
        }

        case IK_CONTENT_TOOL_RESULT: { // LCOV_EXCL_BR_LINE
            // Build functionResponse object
            yyjson_mut_val *func_resp = yyjson_mut_obj(doc);
            if (!func_resp) return false; // LCOV_EXCL_BR_LINE

            // Find the actual function name by looking up the tool_call_id in previous messages
            const char *func_name = find_function_name_for_tool_call(
                messages, message_count, current_msg_idx, block->data.tool_result.tool_call_id);

            // Use function name if found, otherwise fall back to tool_call_id
            const char *name_to_use = (func_name != NULL) ? func_name : block->data.tool_result.tool_call_id;

            if (!yyjson_mut_obj_add_str_(doc, func_resp, "name", name_to_use)) {
                return false;
            }

            // Build response object with content field
            yyjson_mut_val *response_obj = yyjson_mut_obj(doc);
            if (!response_obj) return false; // LCOV_EXCL_BR_LINE

            if (!yyjson_mut_obj_add_str_(doc, response_obj, "content", block->data.tool_result.content)) {
                return false;
            }

            if (!yyjson_mut_obj_add_val_(doc, func_resp, "response", response_obj)) {
                return false;
            }

            if (!yyjson_mut_obj_add_val_(doc, obj, "functionResponse", func_resp)) {
                return false; // LCOV_EXCL_BR_LINE
            }
            break;
        }

        default: // LCOV_EXCL_LINE
            return false; // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_arr_add_val_(arr, obj)) return false; // LCOV_EXCL_BR_LINE
    return true;
}

/**
 * Extract thought signature from provider_metadata JSON
 *
 * @param metadata JSON string containing provider_metadata
 * @return         Thought signature string, or NULL if not found
 *
 * The returned string is owned by the parsed JSON document and
 * must be copied before the document is freed.
 */
const char *ik_google_extract_thought_signature(const char *metadata, yyjson_doc **out_doc)
{
    assert(out_doc != NULL); // LCOV_EXCL_BR_LINE

    *out_doc = NULL;

    if (metadata == NULL || metadata[0] == '\0') {
        return NULL;
    }

    yyjson_doc *doc = yyjson_read(metadata, strlen(metadata), 0);
    if (!doc) {
        // Malformed JSON - log warning but continue
        return NULL;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!root || !yyjson_is_obj(root)) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return NULL;
    }

    yyjson_val *sig = yyjson_obj_get(root, "thought_signature");
    if (!sig || !yyjson_is_str(sig)) { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return NULL;
    }

    const char *sig_str = yyjson_get_str(sig);
    if (sig_str == NULL || sig_str[0] == '\0') { // LCOV_EXCL_BR_LINE
        yyjson_doc_free(doc);
        return NULL;
    }

    *out_doc = doc;
    return sig_str;
}

/**
 * Find most recent thought signature in messages
 *
 * Iterates messages in reverse to find the most recent ASSISTANT message
 * with a thought_signature in provider_metadata.
 *
 * @param req Internal request structure
 * @param out_doc Output: parsed JSON document (caller must free)
 * @return    Thought signature string, or NULL if not found
 */
const char *ik_google_find_latest_thought_signature(const ik_request_t *req, yyjson_doc **out_doc)
{
    assert(req != NULL);     // LCOV_EXCL_BR_LINE
    assert(out_doc != NULL); // LCOV_EXCL_BR_LINE

    *out_doc = NULL;

    // Only process for Gemini 3 models (optimization)
    if (ik_google_model_series(req->model) != IK_GEMINI_3) {
        return NULL;
    }

    // Iterate messages in reverse order to find most recent signature
    for (size_t i = req->message_count; i > 0; i--) {
        const ik_message_t *msg = &req->messages[i - 1];

        // Only check ASSISTANT messages
        if (msg->role != IK_ROLE_ASSISTANT) {
            continue;
        }

        const char *sig = ik_google_extract_thought_signature(msg->provider_metadata, out_doc);
        if (sig != NULL) {
            return sig;
        }
    }

    return NULL;
}

/**
 * Serialize message parts array
 */
bool ik_google_serialize_message_parts(yyjson_mut_doc *doc, yyjson_mut_val *content_obj,
                                       const ik_message_t *message, const char *thought_sig,
                                       bool is_first_assistant, const char *model,
                                       const ik_message_t *messages, size_t message_count,
                                       size_t current_msg_idx)
{
    assert(doc != NULL);         // LCOV_EXCL_BR_LINE
    assert(content_obj != NULL); // LCOV_EXCL_BR_LINE
    assert(message != NULL);     // LCOV_EXCL_BR_LINE

    yyjson_mut_val *parts_arr = yyjson_mut_arr(doc);
    if (!parts_arr) return false; // LCOV_EXCL_BR_LINE

    // Add thought signature as first part if present and this is first assistant message
    if (thought_sig != NULL && is_first_assistant) {
        yyjson_mut_val *sig_obj = yyjson_mut_obj(doc);
        if (!sig_obj) return false; // LCOV_EXCL_BR_LINE

        if (!yyjson_mut_obj_add_str_(doc, sig_obj, "thoughtSignature", thought_sig)) {
            return false;
        }

        if (!yyjson_mut_arr_add_val_(parts_arr, sig_obj)) {
            return false;
        }
    }

    // Add regular content blocks
    for (size_t i = 0; i < message->content_count; i++) {
        if (!ik_google_serialize_content_block(doc, parts_arr, &message->content_blocks[i],
                                                model, messages, message_count, current_msg_idx)) {
            return false;
        }
    }

    if (!yyjson_mut_obj_add_val_(doc, content_obj, "parts", parts_arr)) {
        return false;
    }

    return true;
}
