/**
 * @file request.c
 * @brief Anthropic request serialization implementation
 *
 * Transforms the canonical ik_request_t format to Anthropic's Messages API format.
 * The canonical format is a superset containing all details any provider might need.
 * This serializer is responsible for:
 * - Converting to Anthropic's messages/content structure
 * - Using input_schema for tool definitions (not OpenAI's parameters format)
 * - Mapping thinking levels to Anthropic's extended thinking format
 * - Handling tool_use and tool_result content blocks
 */

#include "request.h"
#include "request_serialize.h"
#include "thinking.h"
#include "error.h"
#include "panic.h"
#include "vendor/yyjson/yyjson.h"
#include <string.h>
#include <assert.h>

// Forward declarations
static bool serialize_thinking(yyjson_mut_doc *doc, yyjson_mut_val *root,
                                const ik_request_t *req);
static bool serialize_tools(yyjson_mut_doc *doc, yyjson_mut_val *root,
                             const ik_request_t *req);
static res_t serialize_request_internal(TALLOC_CTX *ctx, const ik_request_t *req,
                                         bool stream, char **out_json);

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

/**
 * Internal serialize request implementation
 */
static res_t serialize_request_internal(TALLOC_CTX *ctx, const ik_request_t *req,
                                         bool stream, char **out_json)
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

    // Add max_tokens
    // API requires: budget_tokens < max_tokens
    // So max_tokens must be at least thinking_budget + 1
    int32_t max_tokens = req->max_output_tokens;
    if (max_tokens <= 0) {
        max_tokens = 4096;
    }

    // Ensure max_tokens > thinking budget when thinking is enabled
    if (req->thinking.level != IK_THINKING_NONE) {
        int32_t budget = ik_anthropic_thinking_budget(req->model, req->thinking.level);
        if (budget > 0 && max_tokens <= budget) {
            // Set max_tokens to budget + reasonable response buffer
            max_tokens = budget + 4096;
        }
    }

    if (!yyjson_mut_obj_add_int(doc, root, "max_tokens", max_tokens)) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        return ERR(ctx, PARSE, "Failed to add max_tokens field"); // LCOV_EXCL_LINE
    }

    // Add stream flag if streaming
    if (stream) {
        if (!yyjson_mut_obj_add_bool(doc, root, "stream", true)) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            return ERR(ctx, PARSE, "Failed to add stream field"); // LCOV_EXCL_LINE
        }
    }

    // Add system prompt if present
    if (req->system_prompt != NULL) {
        if (!yyjson_mut_obj_add_str(doc, root, "system", req->system_prompt)) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            return ERR(ctx, PARSE, "Failed to add system field"); // LCOV_EXCL_LINE
        }
    }

    // Add messages
    if (!ik_anthropic_serialize_messages(doc, root, req)) {
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

res_t ik_anthropic_serialize_request(TALLOC_CTX *ctx, const ik_request_t *req, char **out_json)
{
    return serialize_request_internal(ctx, req, false, out_json);
}

res_t ik_anthropic_serialize_request_stream(TALLOC_CTX *ctx, const ik_request_t *req, char **out_json)
{
    return serialize_request_internal(ctx, req, true, out_json);
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
