/**
 * @file request_responses.c
 * @brief OpenAI Responses API request serialization implementation
 */

#include "request.h"
#include "reasoning.h"
#include "error.h"
#include "panic.h"
#include "vendor/yyjson/yyjson.h"
#include <string.h>
#include <assert.h>

/* ================================================================
 * Helper Functions
 * ================================================================ */

/**
 * Map internal role to OpenAI role string
 */
static const char *get_openai_role(ik_role_t role)
{
    switch (role) {
        case IK_ROLE_USER:
            return "user";
        case IK_ROLE_ASSISTANT:
            return "assistant";
        case IK_ROLE_TOOL:
            return "tool";
        default: // LCOV_EXCL_LINE
            return "user"; // LCOV_EXCL_LINE
    }
}

/**
 * Serialize a single message for input array
 */
static bool serialize_input_message(yyjson_mut_doc *doc, yyjson_mut_val *input_arr,
                                     const ik_message_t *message)
{
    assert(doc != NULL);       // LCOV_EXCL_BR_LINE
    assert(input_arr != NULL); // LCOV_EXCL_BR_LINE
    assert(message != NULL);   // LCOV_EXCL_BR_LINE

    yyjson_mut_val *msg_obj = yyjson_mut_obj(doc);
    if (!msg_obj) return false; // LCOV_EXCL_BR_LINE

    // Add role
    const char *role_str = get_openai_role(message->role);
    if (!yyjson_mut_obj_add_str(doc, msg_obj, "role", role_str)) {
        return false; // LCOV_EXCL_BR_LINE
    }

    // Concatenate text content for this message
    char *content = talloc_strdup(doc, "");
    if (!content) return false; // LCOV_EXCL_BR_LINE

    for (size_t i = 0; i < message->content_count; i++) {
        if (message->content_blocks[i].type == IK_CONTENT_TEXT) {
            if (strlen(content) > 0) {
                content = talloc_asprintf_append_buffer(content, "\n\n%s",
                                                          message->content_blocks[i].data.text.text);
            } else {
                content = talloc_asprintf_append_buffer(content, "%s",
                                                          message->content_blocks[i].data.text.text);
            }
            if (!content) return false; // LCOV_EXCL_BR_LINE
        }
    }

    if (!yyjson_mut_obj_add_str(doc, msg_obj, "content", content)) {
        return false; // LCOV_EXCL_BR_LINE
    }
    talloc_free(content);

    // Add to array
    if (!yyjson_mut_arr_add_val(input_arr, msg_obj)) {
        return false; // LCOV_EXCL_BR_LINE
    }

    return true;
}

/**
 * Serialize a single tool definition to Responses API format
 * (Same nested format as Chat Completions)
 */
static bool serialize_responses_tool(yyjson_mut_doc *doc, yyjson_mut_val *tools_arr,
                                       const ik_tool_def_t *tool)
{
    assert(doc != NULL);       // LCOV_EXCL_BR_LINE
    assert(tools_arr != NULL); // LCOV_EXCL_BR_LINE
    assert(tool != NULL);      // LCOV_EXCL_BR_LINE

    yyjson_mut_val *tool_obj = yyjson_mut_obj(doc);
    if (!tool_obj) return false; // LCOV_EXCL_BR_LINE

    // Add type
    if (!yyjson_mut_obj_add_str(doc, tool_obj, "type", "function")) {
        return false; // LCOV_EXCL_BR_LINE
    }

    // Create function object
    yyjson_mut_val *func_obj = yyjson_mut_obj(doc);
    if (!func_obj) return false; // LCOV_EXCL_BR_LINE

    // Add name
    if (!yyjson_mut_obj_add_str(doc, func_obj, "name", tool->name)) {
        return false; // LCOV_EXCL_BR_LINE
    }

    // Add description
    if (!yyjson_mut_obj_add_str(doc, func_obj, "description", tool->description)) {
        return false; // LCOV_EXCL_BR_LINE
    }

    // Parse parameters JSON and add as object
    yyjson_doc *params_doc = yyjson_read(tool->parameters,
                                          strlen(tool->parameters), 0);
    if (!params_doc) return false; // LCOV_EXCL_BR_LINE

    yyjson_mut_val *params_mut = yyjson_val_mut_copy(doc, yyjson_doc_get_root(params_doc));
    yyjson_doc_free(params_doc);
    if (!params_mut) return false; // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_val(doc, func_obj, "parameters", params_mut)) {
        return false; // LCOV_EXCL_BR_LINE
    }

    // Add strict: true for structured outputs
    if (!yyjson_mut_obj_add_bool(doc, func_obj, "strict", true)) {
        return false; // LCOV_EXCL_BR_LINE
    }

    // Add function object to tool
    if (!yyjson_mut_obj_add_val(doc, tool_obj, "function", func_obj)) {
        return false; // LCOV_EXCL_BR_LINE
    }

    // Add to array
    if (!yyjson_mut_arr_add_val(tools_arr, tool_obj)) {
        return false; // LCOV_EXCL_BR_LINE
    }

    return true;
}

/**
 * Add tool_choice field to request
 */
static bool add_tool_choice(yyjson_mut_doc *doc, yyjson_mut_val *root, int tool_choice_mode)
{
    assert(doc != NULL);  // LCOV_EXCL_BR_LINE
    assert(root != NULL); // LCOV_EXCL_BR_LINE

    const char *choice_str = NULL;
    switch (tool_choice_mode) {
        case 1: // IK_TOOL_NONE
            choice_str = "none";
            break;
        case 0: // IK_TOOL_AUTO (default)
            choice_str = "auto";
            break;
        case 2: // IK_TOOL_REQUIRED
            choice_str = "required";
            break;
        default:
            choice_str = "auto";
            break;
    }

    if (!yyjson_mut_obj_add_str(doc, root, "tool_choice", choice_str)) {
        return false; // LCOV_EXCL_BR_LINE
    }

    return true;
}

/* ================================================================
 * Public API Implementation
 * ================================================================ */

res_t ik_openai_serialize_responses_request(TALLOC_CTX *ctx, const ik_request_t *req,
                                             bool streaming, char **out_json)
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

    // Add instructions field (system prompt)
    if (req->system_prompt != NULL && strlen(req->system_prompt) > 0) {
        if (!yyjson_mut_obj_add_str(doc, root, "instructions", req->system_prompt)) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            return ERR(ctx, PARSE, "Failed to add instructions field"); // LCOV_EXCL_LINE
        }
    }

    // Determine input format: string for single user message, array for multi-turn
    bool use_string_input = (req->message_count == 1 &&
                              req->messages[0].role == IK_ROLE_USER &&
                              req->messages[0].content_count > 0);

    if (use_string_input) {
        // Single user message: concatenate text content into string input
        char *input_text = talloc_strdup(doc, "");
        if (!input_text) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        for (size_t i = 0; i < req->messages[0].content_count; i++) {
            if (req->messages[0].content_blocks[i].type == IK_CONTENT_TEXT) {
                if (strlen(input_text) > 0) {
                    input_text = talloc_asprintf_append_buffer(input_text, "\n\n%s",
                                                                  req->messages[0].content_blocks[i].data.text.text);
                } else {
                    input_text = talloc_asprintf_append_buffer(input_text, "%s",
                                                                  req->messages[0].content_blocks[i].data.text.text);
                }
                if (!input_text) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            }
        }

        if (!yyjson_mut_obj_add_str(doc, root, "input", input_text)) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            return ERR(ctx, PARSE, "Failed to add input field"); // LCOV_EXCL_LINE
        }
        talloc_free(input_text);
    } else {
        // Multi-turn conversation: use array format
        yyjson_mut_val *input_arr = yyjson_mut_arr(doc);
        if (!input_arr) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            PANIC("Out of memory"); // LCOV_EXCL_LINE
        }

        for (size_t i = 0; i < req->message_count; i++) {
            if (!serialize_input_message(doc, input_arr, &req->messages[i])) {
                yyjson_mut_doc_free(doc);
                return ERR(ctx, PARSE, "Failed to serialize message");
            }
        }

        if (!yyjson_mut_obj_add_val(doc, root, "input", input_arr)) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            return ERR(ctx, PARSE, "Failed to add input array"); // LCOV_EXCL_LINE
        }
    }

    // Add max_output_tokens if set
    if (req->max_output_tokens > 0) {
        if (!yyjson_mut_obj_add_int(doc, root, "max_output_tokens", req->max_output_tokens)) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            return ERR(ctx, PARSE, "Failed to add max_output_tokens"); // LCOV_EXCL_LINE
        }
    }

    // Add streaming configuration
    if (streaming) {
        if (!yyjson_mut_obj_add_bool(doc, root, "stream", true)) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            return ERR(ctx, PARSE, "Failed to add stream field"); // LCOV_EXCL_LINE
        }
    }

    // Add reasoning configuration for reasoning models
    if (ik_openai_is_reasoning_model(req->model) && req->thinking.level != IK_THINKING_NONE) {
        const char *effort = ik_openai_reasoning_effort(req->thinking.level);
        if (effort != NULL) {
            yyjson_mut_val *reasoning_obj = yyjson_mut_obj(doc);
            if (!reasoning_obj) { // LCOV_EXCL_BR_LINE
                yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
                PANIC("Out of memory"); // LCOV_EXCL_LINE
            }

            if (!yyjson_mut_obj_add_str(doc, reasoning_obj, "effort", effort)) { // LCOV_EXCL_BR_LINE
                yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
                return ERR(ctx, PARSE, "Failed to add reasoning effort"); // LCOV_EXCL_LINE
            }

            if (!yyjson_mut_obj_add_val(doc, root, "reasoning", reasoning_obj)) { // LCOV_EXCL_BR_LINE
                yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
                return ERR(ctx, PARSE, "Failed to add reasoning object"); // LCOV_EXCL_LINE
            }
        }
    }

    // Add tools if present
    if (req->tool_count > 0) {
        yyjson_mut_val *tools_arr = yyjson_mut_arr(doc);
        if (!tools_arr) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            PANIC("Out of memory"); // LCOV_EXCL_LINE
        }

        for (size_t i = 0; i < req->tool_count; i++) {
            if (!serialize_responses_tool(doc, tools_arr, &req->tools[i])) {
                yyjson_mut_doc_free(doc);
                return ERR(ctx, PARSE, "Failed to serialize tool");
            }
        }

        if (!yyjson_mut_obj_add_val(doc, root, "tools", tools_arr)) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            return ERR(ctx, PARSE, "Failed to add tools array"); // LCOV_EXCL_LINE
        }

        // Add tool_choice
        if (!add_tool_choice(doc, root, req->tool_choice_mode)) {
            yyjson_mut_doc_free(doc);
            return ERR(ctx, PARSE, "Failed to add tool_choice");
        }
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

res_t ik_openai_build_responses_url(TALLOC_CTX *ctx, const char *base_url, char **out_url)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(base_url != NULL); // LCOV_EXCL_BR_LINE
    assert(out_url != NULL);  // LCOV_EXCL_BR_LINE

    char *url = talloc_asprintf(ctx, "%s/v1/responses", base_url);
    if (!url) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    *out_url = url;
    return OK(url);
}
