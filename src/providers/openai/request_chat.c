/**
 * @file request_chat.c
 * @brief OpenAI Chat Completions request serialization implementation
 *
 * Transforms the canonical ik_request_t format to OpenAI's Chat Completions API format.
 * The canonical format is a superset containing all details any provider might need.
 * This serializer is responsible for:
 * - Converting to OpenAI's messages array structure
 * - Setting strict:true on tools (requires additionalProperties:false in schema)
 * - Mapping tool calls to OpenAI's function calling format
 * - Handling reasoning_effort for o-series models
 */

#include "request.h"
#include "serialize.h"
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
 * Ensure all properties are in the required array for OpenAI strict mode.
 * OpenAI's strict mode requires every property to be listed in required[].
 */
static bool ensure_all_properties_required(yyjson_mut_doc *doc, yyjson_mut_val *params)
{
    assert(doc != NULL);    // LCOV_EXCL_BR_LINE
    assert(params != NULL); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *properties = yyjson_mut_obj_get(params, "properties");
    if (!properties || !yyjson_mut_is_obj(properties)) {
        return true; // No properties to require
    }

    // Build new required array with ALL property keys
    yyjson_mut_val *new_required = yyjson_mut_arr(doc);
    if (!new_required) return false; // LCOV_EXCL_BR_LINE

    yyjson_mut_obj_iter iter;
    yyjson_mut_obj_iter_init(properties, &iter);
    yyjson_mut_val *key;
    while ((key = yyjson_mut_obj_iter_next(&iter)) != NULL) {
        const char *key_str = yyjson_mut_get_str(key);
        if (key_str) { // LCOV_EXCL_BR_LINE - JSON object keys are always strings per spec
            if (!yyjson_mut_arr_add_str(doc, new_required, key_str)) { // LCOV_EXCL_BR_LINE
                return false; // LCOV_EXCL_LINE
            }
        }
    }

    // Remove existing required array if present and add new one
    yyjson_mut_obj_remove_key(params, "required");
    if (!yyjson_mut_obj_add_val(doc, params, "required", new_required)) { // LCOV_EXCL_BR_LINE
        return false; // LCOV_EXCL_LINE
    }

    return true;
}

/**
 * Serialize a single tool definition to Chat Completions format
 */
static bool serialize_chat_tool(yyjson_mut_doc *doc, yyjson_mut_val *tools_arr,
                                  const ik_tool_def_t *tool)
{
    assert(doc != NULL);       // LCOV_EXCL_BR_LINE
    assert(tools_arr != NULL); // LCOV_EXCL_BR_LINE
    assert(tool != NULL);      // LCOV_EXCL_BR_LINE

    yyjson_mut_val *tool_obj = yyjson_mut_obj(doc);
    if (!tool_obj) return false; // LCOV_EXCL_BR_LINE

    // Add type
    if (!yyjson_mut_obj_add_str(doc, tool_obj, "type", "function")) { // LCOV_EXCL_BR_LINE
        return false; // LCOV_EXCL_LINE
    }

    // Create function object
    yyjson_mut_val *func_obj = yyjson_mut_obj(doc);
    if (!func_obj) return false; // LCOV_EXCL_BR_LINE

    // Add name
    if (!yyjson_mut_obj_add_str(doc, func_obj, "name", tool->name)) { // LCOV_EXCL_BR_LINE
        return false; // LCOV_EXCL_LINE
    }

    // Add description
    if (!yyjson_mut_obj_add_str(doc, func_obj, "description", tool->description)) { // LCOV_EXCL_BR_LINE
        return false; // LCOV_EXCL_LINE
    }

    // Parse parameters JSON and add as object
    yyjson_doc *params_doc = yyjson_read(tool->parameters,
                                          strlen(tool->parameters), 0);
    if (!params_doc) return false;

    yyjson_mut_val *params_mut = yyjson_val_mut_copy(doc, yyjson_doc_get_root(params_doc));
    yyjson_doc_free(params_doc);
    if (!params_mut) return false; // LCOV_EXCL_BR_LINE

    // OpenAI strict mode requires ALL properties in the required array
    if (!ensure_all_properties_required(doc, params_mut)) { // LCOV_EXCL_BR_LINE
        return false; // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_obj_add_val(doc, func_obj, "parameters", params_mut)) { // LCOV_EXCL_BR_LINE
        return false; // LCOV_EXCL_LINE
    }

    // Add strict: true for structured outputs
    if (!yyjson_mut_obj_add_bool(doc, func_obj, "strict", true)) { // LCOV_EXCL_BR_LINE
        return false; // LCOV_EXCL_LINE
    }

    // Add function object to tool
    if (!yyjson_mut_obj_add_val(doc, tool_obj, "function", func_obj)) { // LCOV_EXCL_BR_LINE
        return false; // LCOV_EXCL_LINE
    }

    // Add to array
    if (!yyjson_mut_arr_add_val(tools_arr, tool_obj)) { // LCOV_EXCL_BR_LINE
        return false; // LCOV_EXCL_LINE
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

    if (!yyjson_mut_obj_add_str(doc, root, "tool_choice", choice_str)) { // LCOV_EXCL_BR_LINE
        return false; // LCOV_EXCL_LINE
    }

    return true;
}

/* ================================================================
 * Public API Implementation
 * ================================================================ */

res_t ik_openai_serialize_chat_request(TALLOC_CTX *ctx, const ik_request_t *req,
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

    // Create messages array
    yyjson_mut_val *messages_arr = yyjson_mut_arr(doc);
    if (!messages_arr) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    // Add system message if system_prompt is present
    if (req->system_prompt != NULL && strlen(req->system_prompt) > 0) {
        yyjson_mut_val *sys_msg = yyjson_mut_obj(doc);
        if (!sys_msg) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            PANIC("Out of memory"); // LCOV_EXCL_LINE
        }

        if (!yyjson_mut_obj_add_str(doc, sys_msg, "role", "system")) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            return ERR(ctx, PARSE, "Failed to add system role"); // LCOV_EXCL_LINE
        }

        if (!yyjson_mut_obj_add_str(doc, sys_msg, "content", req->system_prompt)) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            return ERR(ctx, PARSE, "Failed to add system content"); // LCOV_EXCL_LINE
        }

        if (!yyjson_mut_arr_add_val(messages_arr, sys_msg)) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            return ERR(ctx, PARSE, "Failed to add system message"); // LCOV_EXCL_LINE
        }
    }

    // Add conversation messages
    for (size_t i = 0; i < req->message_count; i++) {
        yyjson_mut_val *msg_obj = ik_openai_serialize_message(doc, &req->messages[i]);
        if (!yyjson_mut_arr_add_val(messages_arr, msg_obj)) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            return ERR(ctx, PARSE, "Failed to add message to array"); // LCOV_EXCL_LINE
        }
    }

    // Add messages array to root
    if (!yyjson_mut_obj_add_val(doc, root, "messages", messages_arr)) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        return ERR(ctx, PARSE, "Failed to add messages array"); // LCOV_EXCL_LINE
    }

    // Add max_completion_tokens if set
    if (req->max_output_tokens > 0) {
        if (!yyjson_mut_obj_add_int(doc, root, "max_completion_tokens", req->max_output_tokens)) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            return ERR(ctx, PARSE, "Failed to add max_completion_tokens"); // LCOV_EXCL_LINE
        }
    }

    // Add streaming configuration
    if (streaming) {
        if (!yyjson_mut_obj_add_bool(doc, root, "stream", true)) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            return ERR(ctx, PARSE, "Failed to add stream field"); // LCOV_EXCL_LINE
        }

        // Add stream_options for usage tracking
        yyjson_mut_val *stream_opts = yyjson_mut_obj(doc);
        if (!stream_opts) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            PANIC("Out of memory"); // LCOV_EXCL_LINE
        }

        if (!yyjson_mut_obj_add_bool(doc, stream_opts, "include_usage", true)) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            return ERR(ctx, PARSE, "Failed to add include_usage"); // LCOV_EXCL_LINE
        }

        if (!yyjson_mut_obj_add_val(doc, root, "stream_options", stream_opts)) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            return ERR(ctx, PARSE, "Failed to add stream_options"); // LCOV_EXCL_LINE
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
            if (!serialize_chat_tool(doc, tools_arr, &req->tools[i])) {
                yyjson_mut_doc_free(doc);
                return ERR(ctx, PARSE, "Failed to serialize tool");
            }
        }

        if (!yyjson_mut_obj_add_val(doc, root, "tools", tools_arr)) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            return ERR(ctx, PARSE, "Failed to add tools array"); // LCOV_EXCL_LINE
        }

        // Add tool_choice
        if (!add_tool_choice(doc, root, req->tool_choice_mode)) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            return ERR(ctx, PARSE, "Failed to add tool_choice"); // LCOV_EXCL_LINE
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

res_t ik_openai_build_chat_url(TALLOC_CTX *ctx, const char *base_url, char **out_url)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(base_url != NULL); // LCOV_EXCL_BR_LINE
    assert(out_url != NULL);  // LCOV_EXCL_BR_LINE

    char *url = talloc_asprintf(ctx, "%s/v1/chat/completions", base_url);
    if (!url) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    *out_url = url;
    return OK(url);
}

res_t ik_openai_build_headers(TALLOC_CTX *ctx, const char *api_key, char ***out_headers)
{
    assert(ctx != NULL);        // LCOV_EXCL_BR_LINE
    assert(api_key != NULL);    // LCOV_EXCL_BR_LINE
    assert(out_headers != NULL); // LCOV_EXCL_BR_LINE

    // Allocate array of 3 strings (2 headers + NULL)
    char **headers = talloc_array(ctx, char *, 3);
    if (!headers) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Build Authorization header
    headers[0] = talloc_asprintf(headers, "Authorization: Bearer %s", api_key);
    if (!headers[0]) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Build Content-Type header
    headers[1] = talloc_strdup(headers, "Content-Type: application/json");
    if (!headers[1]) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // NULL terminator
    headers[2] = NULL;

    *out_headers = headers;
    return OK(headers);
}
