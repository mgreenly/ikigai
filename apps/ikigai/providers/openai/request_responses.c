/**
 * @file request_responses.c
 * @brief OpenAI Responses API request serialization implementation
 */

#include "apps/ikigai/providers/openai/request.h"
#include "apps/ikigai/providers/openai/serialize.h"
#include "apps/ikigai/providers/openai/reasoning.h"
#include "apps/ikigai/providers/openai/error.h"
#include "shared/panic.h"
#include "shared/wrapper.h"
#include "vendor/yyjson/yyjson.h"
#include <string.h>
#include <assert.h>


#include "shared/poison.h"
/* ================================================================
 * Helper Functions
 * ================================================================ */

/**
 * Remove "format" fields from schema recursively.
 * OpenAI rejects certain format validators like "uri" that are valid JSON Schema.
 * This function recursively walks the schema and removes all "format" fields.
 */
static void remove_format_validators(yyjson_mut_val *schema)
{
    if (!schema || !yyjson_mut_is_obj(schema)) {
        return;
    }

    // Remove format field if present
    yyjson_mut_obj_remove_key(schema, "format");

    // Recursively process properties object
    yyjson_mut_val *properties = yyjson_mut_obj_get(schema, "properties");
    if (properties && yyjson_mut_is_obj(properties)) {
        yyjson_mut_obj_iter iter;
        yyjson_mut_obj_iter_init(properties, &iter);
        yyjson_mut_val *key;
        while ((key = yyjson_mut_obj_iter_next(&iter)) != NULL) {
            yyjson_mut_val *value = yyjson_mut_obj_iter_get_val(key);
            remove_format_validators(value);
        }
    }

    // Recursively process items (for arrays)
    yyjson_mut_val *items = yyjson_mut_obj_get(schema, "items");
    if (items && yyjson_mut_is_obj(items)) {
        remove_format_validators(items);
    }

    // Recursively process oneOf/anyOf/allOf arrays
    const char *combinators[] = {"oneOf", "anyOf", "allOf"};
    for (size_t i = 0; i < 3; i++) {
        yyjson_mut_val *combinator = yyjson_mut_obj_get(schema, combinators[i]);
        if (combinator && yyjson_mut_is_arr(combinator)) {
            yyjson_mut_arr_iter arr_iter;
            yyjson_mut_arr_iter_init(combinator, &arr_iter);
            yyjson_mut_val *elem;
            while ((elem = yyjson_mut_arr_iter_next(&arr_iter)) != NULL) {
                remove_format_validators(elem);
            }
        }
    }
}

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
 * Serialize a single tool definition to Responses API format
 * (Flat format - no function wrapper)
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
    if (!yyjson_mut_obj_add_str_(doc, tool_obj, "type", "function")) {
        return false;
    }

    // Add name
    if (!yyjson_mut_obj_add_str_(doc, tool_obj, "name", tool->name)) {
        return false;
    }

    // Add description
    if (!yyjson_mut_obj_add_str_(doc, tool_obj, "description", tool->description)) {
        return false;
    }

    // Parse parameters JSON and add as object
    yyjson_doc *params_doc = yyjson_read(tool->parameters,
                                         strlen(tool->parameters), 0);
    if (!params_doc) return false; // LCOV_EXCL_BR_LINE

    yyjson_mut_val *params_mut = yyjson_val_mut_copy(doc, yyjson_doc_get_root(params_doc));
    yyjson_doc_free(params_doc);
    if (!params_mut) return false; // LCOV_EXCL_BR_LINE

    // Remove format validators that OpenAI doesn't support (e.g., "uri")
    remove_format_validators(params_mut);

    // OpenAI strict mode requires ALL properties in the required array
    if (!ensure_all_properties_required(doc, params_mut)) { // LCOV_EXCL_BR_LINE
        return false; // LCOV_EXCL_LINE
    }

    // OpenAI strict mode requires additionalProperties: false
    if (!yyjson_mut_obj_add_bool(doc, params_mut, "additionalProperties", false)) { // LCOV_EXCL_BR_LINE
        return false; // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_obj_add_val_(doc, tool_obj, "parameters", params_mut)) {
        return false;
    }

    // Add strict: true for structured outputs
    if (!yyjson_mut_obj_add_bool_(doc, tool_obj, "strict", true)) {
        return false;
    }

    // Add to array
    if (!yyjson_mut_arr_add_val_(tools_arr, tool_obj)) {
        return false;
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

    if (!yyjson_mut_obj_add_str_(doc, root, "tool_choice", choice_str)) {
        return false;
    }

    return true;
}

/* ================================================================
 * Request Building Helpers
 * ================================================================ */

static char *build_string_input(const ik_message_t *msg)
{
    size_t total_len = 0;
    for (size_t i = 0; i < msg->content_count; i++) {
        if (msg->content_blocks[i].type == IK_CONTENT_TEXT) {
            if (total_len > 0) total_len += 2;
            total_len += strlen(msg->content_blocks[i].data.text.text);
        }
    }

    if (total_len == 0) {
        return NULL;
    }

    char *input_text = malloc(total_len + 1);
    if (!input_text) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    char *dest = input_text;
    bool first = true;
    for (size_t i = 0; i < msg->content_count; i++) {
        if (msg->content_blocks[i].type == IK_CONTENT_TEXT) {
            if (!first) {
                memcpy(dest, "\n\n", 2);
                dest += 2;
            }
            size_t text_len = strlen(msg->content_blocks[i].data.text.text);
            memcpy(dest, msg->content_blocks[i].data.text.text, text_len);
            dest += text_len;
            first = false;
        }
    }
    *dest = '\0';

    return input_text;
}

static bool add_string_input(yyjson_mut_doc *doc, yyjson_mut_val *root,
                             const ik_message_t *msg)
{
    char *input_text = build_string_input(msg);

    if (input_text == NULL) {
        if (!yyjson_mut_obj_add_str(doc, root, "input", "")) { // LCOV_EXCL_BR_LINE
            return false; // LCOV_EXCL_LINE
        }
        return true;
    }

    bool result = yyjson_mut_obj_add_strcpy(doc, root, "input", input_text); // LCOV_EXCL_BR_LINE
    free(input_text);
    return result;
}

static bool add_array_input(yyjson_mut_doc *doc, yyjson_mut_val *root,
                            const ik_request_t *req)
{
    yyjson_mut_val *input_arr = yyjson_mut_arr(doc);
    if (!input_arr) { // LCOV_EXCL_BR_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    for (size_t i = 0; i < req->message_count; i++) {
        if (!ik_openai_serialize_responses_message(doc, &req->messages[i], input_arr)) { // LCOV_EXCL_BR_LINE
            return false; // LCOV_EXCL_LINE
        }
    }

    if (!yyjson_mut_obj_add_val(doc, root, "input", input_arr)) { // LCOV_EXCL_BR_LINE
        return false; // LCOV_EXCL_LINE
    }

    return true;
}

static bool add_input_field(yyjson_mut_doc *doc, yyjson_mut_val *root,
                           const ik_request_t *req)
{
    bool use_string_input = (req->message_count == 1 &&
                             req->messages[0].role == IK_ROLE_USER &&
                             req->messages[0].content_count > 0);

    if (use_string_input) {
        return add_string_input(doc, root, &req->messages[0]);
    }

    return add_array_input(doc, root, req);
}

static bool add_reasoning_config(yyjson_mut_doc *doc, yyjson_mut_val *root, const ik_request_t *req)
{
    if (!ik_openai_is_reasoning_model(req->model)) {
        return true;
    }

    const char *effort = ik_openai_reasoning_effort(req->model, req->thinking.level);
    if (effort == NULL) {
        return true;
    }

    yyjson_mut_val *reasoning_obj = yyjson_mut_obj(doc);
    if (!reasoning_obj) { // LCOV_EXCL_BR_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_obj_add_str(doc, reasoning_obj, "effort", effort)) { // LCOV_EXCL_BR_LINE
        return false; // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_obj_add_val(doc, root, "reasoning", reasoning_obj)) { // LCOV_EXCL_BR_LINE
        return false; // LCOV_EXCL_LINE
    }

    return true;
}

static bool add_tools_and_choice(yyjson_mut_doc *doc, yyjson_mut_val *root,
                                const ik_request_t *req)
{
    if (req->tool_count == 0) {
        return true;
    }

    yyjson_mut_val *tools_arr = yyjson_mut_arr(doc);
    if (!tools_arr) { // LCOV_EXCL_BR_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    for (size_t i = 0; i < req->tool_count; i++) {
        if (!serialize_responses_tool(doc, tools_arr, &req->tools[i])) {
            return false;
        }
    }

    if (!yyjson_mut_obj_add_val(doc, root, "tools", tools_arr)) { // LCOV_EXCL_BR_LINE
        return false; // LCOV_EXCL_LINE
    }

    if (!add_tool_choice(doc, root, req->tool_choice_mode)) {
        return false;
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

    if (req->model == NULL) {
        return ERR(ctx, INVALID_ARG, "Model cannot be NULL");
    }

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (!root) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_obj_add_str(doc, root, "model", req->model)) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        return ERR(ctx, PARSE, "Failed to add model field"); // LCOV_EXCL_LINE
    }

    if (req->system_prompt != NULL && strlen(req->system_prompt) > 0) {
        if (!yyjson_mut_obj_add_str(doc, root, "instructions", req->system_prompt)) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            return ERR(ctx, PARSE, "Failed to add instructions field"); // LCOV_EXCL_LINE
        }
    }

    if (!add_input_field(doc, root, req)) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        return ERR(ctx, PARSE, "Failed to add input field"); // LCOV_EXCL_LINE
    }

    if (req->max_output_tokens > 0) {
        if (!yyjson_mut_obj_add_int(doc, root, "max_output_tokens", req->max_output_tokens)) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            return ERR(ctx, PARSE, "Failed to add max_output_tokens"); // LCOV_EXCL_LINE
        }
    }

    if (streaming) {
        if (!yyjson_mut_obj_add_bool(doc, root, "stream", true)) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            return ERR(ctx, PARSE, "Failed to add stream field"); // LCOV_EXCL_LINE
        }
    }

    if (!add_reasoning_config(doc, root, req)) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        return ERR(ctx, PARSE, "Failed to add reasoning config"); // LCOV_EXCL_LINE
    }

    if (!add_tools_and_choice(doc, root, req)) {
        yyjson_mut_doc_free(doc);
        return ERR(ctx, PARSE, "Failed to add tools");
    }

    yyjson_mut_doc_set_root(doc, root);

    char *json_str = yyjson_mut_write(doc, 0, NULL);
    if (!json_str) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    char *result = talloc_strdup(ctx, json_str);
    if (!result) { // LCOV_EXCL_BR_LINE
        free(json_str); // LCOV_EXCL_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

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
