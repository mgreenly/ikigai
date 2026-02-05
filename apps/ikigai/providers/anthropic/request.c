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

#include "apps/ikigai/providers/anthropic/request.h"
#include "apps/ikigai/providers/anthropic/request_serialize.h"
#include "apps/ikigai/providers/anthropic/thinking.h"
#include "apps/ikigai/providers/anthropic/error.h"
#include "shared/panic.h"
#include "vendor/yyjson/yyjson.h"
#include <string.h>
#include <assert.h>


#include "shared/poison.h"
// Helper: calculate max_tokens with thinking budget adjustment
static int32_t calculate_max_tokens(const ik_request_t *req)
{
    int32_t max_tokens = req->max_output_tokens;
    if (max_tokens <= 0) {
        max_tokens = 4096;
    }

    // Only adjust for budget-based models, not adaptive models
    if (req->thinking.level != IK_THINKING_NONE && !ik_anthropic_is_adaptive_model(req->model)) {
        int32_t budget = ik_anthropic_thinking_budget(req->model, req->thinking.level);
        if (budget > 0 && max_tokens <= budget) {
            max_tokens = budget + 4096;
        }
    }

    return max_tokens;
}

// Helper: add thinking configuration to request
static void add_thinking_config(yyjson_mut_doc *doc, yyjson_mut_val *root,
                                const ik_request_t *req)
{
    if (req->thinking.level == IK_THINKING_NONE) {
        return;
    }

    // Check if model uses adaptive thinking (effort-based)
    if (ik_anthropic_is_adaptive_model(req->model)) {
        const char *effort = ik_anthropic_thinking_effort(req->model, req->thinking.level);
        if (effort == NULL) {
            return; // Omit thinking parameter
        }

        // Adaptive thinking: {"thinking": {"type": "adaptive"}}
        yyjson_mut_val *thinking_obj = yyjson_mut_obj(doc);
        if (!thinking_obj) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        if (!yyjson_mut_obj_add_str(doc, thinking_obj, "type", "adaptive")) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            PANIC("Out of memory"); // LCOV_EXCL_LINE
        }

        if (!yyjson_mut_obj_add_val(doc, root, "thinking", thinking_obj)) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            PANIC("Out of memory"); // LCOV_EXCL_LINE
        }

        // Effort goes in output_config, not inside thinking
        yyjson_mut_val *output_config = yyjson_mut_obj(doc);
        if (!output_config) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        if (!yyjson_mut_obj_add_str(doc, output_config, "effort", effort)) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            PANIC("Out of memory"); // LCOV_EXCL_LINE
        }

        if (!yyjson_mut_obj_add_val(doc, root, "output_config", output_config)) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            PANIC("Out of memory"); // LCOV_EXCL_LINE
        }

        return;
    }

    // Budget-based thinking (sonnet-4-5, haiku-4-5, opus-4-5)
    int32_t budget = ik_anthropic_thinking_budget(req->model, req->thinking.level);
    if (budget == -1) {
        return;
    }

    yyjson_mut_val *thinking_obj = yyjson_mut_obj(doc);
    if (!thinking_obj) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, thinking_obj, "type", "enabled")) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_obj_add_int(doc, thinking_obj, "budget_tokens", budget)) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_obj_add_val(doc, root, "thinking", thinking_obj)) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }
}

// Helper: serialize single tool definition
static res_t serialize_tool(TALLOC_CTX *ctx, yyjson_mut_doc *doc,
                            yyjson_mut_val *tools_arr,
                            const ik_tool_def_t *tool)
{
    yyjson_mut_val *tool_obj = yyjson_mut_obj(doc);
    if (!tool_obj) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, tool_obj, "name", tool->name)) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_obj_add_str(doc, tool_obj, "description", tool->description)) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    yyjson_doc *params_doc = yyjson_read(tool->parameters,
                                         strlen(tool->parameters), 0);
    if (!params_doc) {
        yyjson_mut_doc_free(doc);
        return ERR(ctx, PARSE, "Invalid tool parameters JSON");
    }

    yyjson_mut_val *params_mut = yyjson_val_mut_copy(doc, yyjson_doc_get_root(params_doc));
    yyjson_doc_free(params_doc);
    if (!params_mut) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_val(doc, tool_obj, "input_schema", params_mut)) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_arr_add_val(tools_arr, tool_obj)) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    return OK(NULL);
}

// Helper: map tool choice mode to Anthropic type string
static const char *map_tool_choice_type(int32_t tool_choice_mode)
{
    switch (tool_choice_mode) {
        case 1: return "none";
        case 0: return "auto";
        case 2: return "any";
        default: return "auto";
    }
}

// Helper: add tool_choice configuration
static void add_tool_choice(yyjson_mut_doc *doc, yyjson_mut_val *root,
                           const ik_request_t *req)
{
    yyjson_mut_val *tool_choice_obj = yyjson_mut_obj(doc);
    if (!tool_choice_obj) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    const char *choice_type = map_tool_choice_type(req->tool_choice_mode);

    if (!yyjson_mut_obj_add_str(doc, tool_choice_obj, "type", choice_type)) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_obj_add_val(doc, root, "tool_choice", tool_choice_obj)) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }
}

// Helper: add tools array to request
static res_t add_tools(TALLOC_CTX *ctx, yyjson_mut_doc *doc,
                      yyjson_mut_val *root, const ik_request_t *req)
{
    if (req->tool_count == 0) {
        return OK(NULL);
    }

    yyjson_mut_val *tools_arr = yyjson_mut_arr(doc);
    if (!tools_arr) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    for (size_t i = 0; i < req->tool_count; i++) {
        res_t res = serialize_tool(ctx, doc, tools_arr, &req->tools[i]);
        if (is_err(&res)) {
            return res;
        }
    }

    if (!yyjson_mut_obj_add_val(doc, root, "tools", tools_arr)) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    add_tool_choice(doc, root, req);
    return OK(NULL);
}

/**
 * Internal serialize request implementation
 */
static res_t serialize_request_internal(TALLOC_CTX *ctx, const ik_request_t *req,
                                        char **out_json)
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

    int32_t max_tokens = calculate_max_tokens(req);
    if (!yyjson_mut_obj_add_int(doc, root, "max_tokens", max_tokens)) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        return ERR(ctx, PARSE, "Failed to add max_tokens field"); // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_obj_add_bool(doc, root, "stream", true)) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        return ERR(ctx, PARSE, "Failed to add stream field"); // LCOV_EXCL_LINE
    }

    if (req->system_prompt != NULL) {
        if (!yyjson_mut_obj_add_str(doc, root, "system", req->system_prompt)) { // LCOV_EXCL_BR_LINE
            yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
            return ERR(ctx, PARSE, "Failed to add system field"); // LCOV_EXCL_LINE
        }
    }

    if (!ik_anthropic_serialize_messages(doc, root, req)) {
        yyjson_mut_doc_free(doc);
        return ERR(ctx, PARSE, "Failed to serialize messages");
    }

    add_thinking_config(doc, root, req);

    res_t tools_res = add_tools(ctx, doc, root, req);
    if (is_err(&tools_res)) {
        return tools_res;
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

res_t ik_anthropic_serialize_request_stream(TALLOC_CTX *ctx, const ik_request_t *req, char **out_json)
{
    return serialize_request_internal(ctx, req, out_json);
}
