/**
 * @file request.c
 * @brief Anthropic request serialization implementation
 */

#include "request.h"
#include "thinking.h"
#include "error.h"
#include "panic.h"
#include "vendor/yyjson/yyjson.h"
#include <string.h>
#include <assert.h>

/**
 * Serialize a single content block to Anthropic JSON format
 */
static bool serialize_content_block(yyjson_mut_doc *doc, yyjson_mut_val *arr,
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
static bool serialize_message_content(yyjson_mut_doc *doc, yyjson_mut_val *msg_obj,
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
        if (!serialize_content_block(doc, content_arr, &message->content_blocks[i])) {
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
static const char *role_to_string(ik_role_t role)
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
static bool serialize_messages(yyjson_mut_doc *doc, yyjson_mut_val *root,
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
        const char *role_str = role_to_string(req->messages[i].role);
        if (!yyjson_mut_obj_add_str(doc, msg_obj, "role", role_str)) {
            return false; // LCOV_EXCL_BR_LINE
        }

        // Add content
        if (!serialize_message_content(doc, msg_obj, &req->messages[i])) {
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

/**
 * Serialize thinking configuration
 */
static bool serialize_thinking(yyjson_mut_doc *doc, yyjson_mut_val *root,
                                 const ik_request_t *req)
{
    assert(doc != NULL);  // LCOV_EXCL_BR_LINE
    assert(root != NULL); // LCOV_EXCL_BR_LINE
    assert(req != NULL);  // LCOV_EXCL_BR_LINE

    // Skip if no thinking
    if (req->thinking.level == IK_THINKING_NONE) {
        return true;
    }

    // Calculate budget
    int32_t budget = ik_anthropic_thinking_budget(req->model, req->thinking.level);
    if (budget == -1) {
        // Model doesn't support thinking, skip
        return true;
    }

    // Build thinking config object
    yyjson_mut_val *thinking_obj = yyjson_mut_obj(doc);
    if (!thinking_obj) return false; // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, thinking_obj, "type", "enabled")) {
        return false; // LCOV_EXCL_BR_LINE
    }

    if (!yyjson_mut_obj_add_int(doc, thinking_obj, "budget_tokens", budget)) {
        return false; // LCOV_EXCL_BR_LINE
    }

    if (!yyjson_mut_obj_add_val(doc, root, "thinking", thinking_obj)) {
        return false; // LCOV_EXCL_BR_LINE
    }

    return true;
}

/**
 * Serialize tool definitions
 */
static bool serialize_tools(yyjson_mut_doc *doc, yyjson_mut_val *root,
                              const ik_request_t *req)
{
    assert(doc != NULL);  // LCOV_EXCL_BR_LINE
    assert(root != NULL); // LCOV_EXCL_BR_LINE
    assert(req != NULL);  // LCOV_EXCL_BR_LINE

    // Skip if no tools
    if (req->tool_count == 0) {
        return true;
    }

    yyjson_mut_val *tools_arr = yyjson_mut_arr(doc);
    if (!tools_arr) return false; // LCOV_EXCL_BR_LINE

    for (size_t i = 0; i < req->tool_count; i++) {
        const ik_tool_def_t *tool = &req->tools[i];

        yyjson_mut_val *tool_obj = yyjson_mut_obj(doc);
        if (!tool_obj) return false; // LCOV_EXCL_BR_LINE

        if (!yyjson_mut_obj_add_str(doc, tool_obj, "name", tool->name)) {
            return false; // LCOV_EXCL_BR_LINE
        }

        if (!yyjson_mut_obj_add_str(doc, tool_obj, "description", tool->description)) {
            return false; // LCOV_EXCL_BR_LINE
        }

        // Parse parameters JSON and add as "input_schema"
        yyjson_doc *params_doc = yyjson_read(tool->parameters,
                                              strlen(tool->parameters), 0);
        if (!params_doc) return false; // LCOV_EXCL_BR_LINE

        yyjson_mut_val *params_mut = yyjson_val_mut_copy(doc, yyjson_doc_get_root(params_doc));
        yyjson_doc_free(params_doc);
        if (!params_mut) return false; // LCOV_EXCL_BR_LINE

        if (!yyjson_mut_obj_add_val(doc, tool_obj, "input_schema", params_mut)) {
            return false; // LCOV_EXCL_BR_LINE
        }

        if (!yyjson_mut_arr_add_val(tools_arr, tool_obj)) {
            return false; // LCOV_EXCL_BR_LINE
        }
    }

    if (!yyjson_mut_obj_add_val(doc, root, "tools", tools_arr)) {
        return false; // LCOV_EXCL_BR_LINE
    }

    // Add tool_choice mapping
    yyjson_mut_val *tool_choice_obj = yyjson_mut_obj(doc);
    if (!tool_choice_obj) return false; // LCOV_EXCL_BR_LINE

    // Map tool choice mode
    const char *choice_type = NULL;
    switch (req->tool_choice_mode) {
        case 1: // IK_TOOL_NONE
            choice_type = "none";
            break;
        case 0: // IK_TOOL_AUTO (default)
            choice_type = "auto";
            break;
        case 2: // IK_TOOL_REQUIRED
            choice_type = "any";
            break;
        default:
            choice_type = "auto";
            break;
    }

    if (!yyjson_mut_obj_add_str(doc, tool_choice_obj, "type", choice_type)) {
        return false; // LCOV_EXCL_BR_LINE
    }

    if (!yyjson_mut_obj_add_val(doc, root, "tool_choice", tool_choice_obj)) {
        return false; // LCOV_EXCL_BR_LINE
    }

    return true;
}

res_t ik_anthropic_serialize_request(TALLOC_CTX *ctx, const ik_request_t *req, char **out_json)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(req != NULL);      // LCOV_EXCL_BR_LINE
    assert(out_json != NULL); // LCOV_EXCL_BR_LINE

    // Validate model
    if (req->model == NULL) {
        return ERR(ctx, INVALID_ARG, "Model cannot be NULL");
    }

    // Create JSON document
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (!root) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    // Add model
    if (!yyjson_mut_obj_add_str(doc, root, "model", req->model)) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        return ERR(ctx, PARSE, "Failed to add model field"); // LCOV_EXCL_LINE
    }

    // Add max_tokens (default to 4096 if not set)
    int32_t max_tokens = req->max_output_tokens;
    if (max_tokens <= 0) {
        max_tokens = 4096;
    }
    if (!yyjson_mut_obj_add_int(doc, root, "max_tokens", max_tokens)) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        return ERR(ctx, PARSE, "Failed to add max_tokens field"); // LCOV_EXCL_LINE
    }

    // Add system prompt if present
    if (req->system_prompt != NULL) {
        if (!yyjson_mut_obj_add_str(doc, root, "system", req->system_prompt)) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            return ERR(ctx, PARSE, "Failed to add system field"); // LCOV_EXCL_LINE
        }
    }

    // Add messages
    if (!serialize_messages(doc, root, req)) {
        yyjson_mut_doc_free(doc);
        return ERR(ctx, PARSE, "Failed to serialize messages");
    }

    // Add thinking configuration
    if (!serialize_thinking(doc, root, req)) {
        yyjson_mut_doc_free(doc);
        return ERR(ctx, PARSE, "Failed to serialize thinking config");
    }

    // Add tools
    if (!serialize_tools(doc, root, req)) {
        yyjson_mut_doc_free(doc);
        return ERR(ctx, PARSE, "Failed to serialize tools");
    }

    // Set root and serialize
    yyjson_mut_doc_set_root(doc, root);

    char *json_str = yyjson_mut_write(doc, 0, NULL);
    if (!json_str) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    // Copy to talloc context
    char *result = talloc_strdup(ctx, json_str);
    if (!result) { // LCOV_EXCL_BR_LINE
        free(json_str); // LCOV_EXCL_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    // Cleanup
    free(json_str);
    yyjson_mut_doc_free(doc);

    *out_json = result;
    return OK(result);
}

res_t ik_anthropic_build_headers(TALLOC_CTX *ctx, const char *api_key, char ***out_headers)
{
    assert(ctx != NULL);        // LCOV_EXCL_BR_LINE
    assert(api_key != NULL);    // LCOV_EXCL_BR_LINE
    assert(out_headers != NULL); // LCOV_EXCL_BR_LINE

    // Allocate array of 4 strings (3 headers + NULL)
    char **headers = talloc_array(ctx, char *, 4);
    if (!headers) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Build x-api-key header
    headers[0] = talloc_asprintf(headers, "x-api-key: %s", api_key);
    if (!headers[0]) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Build anthropic-version header
    headers[1] = talloc_strdup(headers, "anthropic-version: 2023-06-01");
    if (!headers[1]) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Build content-type header
    headers[2] = talloc_strdup(headers, "content-type: application/json");
    if (!headers[2]) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // NULL terminator
    headers[3] = NULL;

    *out_headers = headers;
    return OK(headers);
}
