/**
 * @file request.c
 * @brief Google request serialization implementation
 *
 * Transforms the canonical ik_request_t format to Google Gemini's native API format.
 * The canonical format is a superset containing all details any provider might need.
 * This serializer is responsible for:
 * - Converting to Gemini's contents/parts structure
 * - Using functionDeclarations for tools (not OpenAI's function format)
 * - Removing unsupported schema fields (e.g., additionalProperties)
 * - Mapping thinking levels to Gemini's thinkingConfig format
 */

#include "apps/ikigai/providers/google/request.h"
#include "apps/ikigai/providers/google/request_helpers.h"
#include "apps/ikigai/providers/google/thinking.h"
#include "apps/ikigai/providers/google/error.h"
#include "shared/panic.h"
#include "vendor/yyjson/yyjson.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>


#include "shared/poison.h"
/* ================================================================
 * Main Serialization Functions
 * ================================================================ */

/**
 * Serialize system instruction
 */
static bool serialize_system_instruction(yyjson_mut_doc *doc, yyjson_mut_val *root,
                                         const ik_request_t *req)
{
    assert(doc != NULL);  // LCOV_EXCL_BR_LINE
    assert(root != NULL); // LCOV_EXCL_BR_LINE
    assert(req != NULL);  // LCOV_EXCL_BR_LINE

    if (req->system_prompt == NULL || req->system_prompt[0] == '\0') {
        return true;
    }

    yyjson_mut_val *sys_obj = yyjson_mut_obj(doc);
    if (!sys_obj) return false; // LCOV_EXCL_BR_LINE

    yyjson_mut_val *parts_arr = yyjson_mut_arr(doc);
    if (!parts_arr) return false; // LCOV_EXCL_BR_LINE

    yyjson_mut_val *part_obj = yyjson_mut_obj(doc);
    if (!part_obj) return false; // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, part_obj, "text", req->system_prompt)) { // LCOV_EXCL_BR_LINE
        return false; // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_arr_add_val(parts_arr, part_obj)) { // LCOV_EXCL_BR_LINE
        return false; // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_obj_add_val(doc, sys_obj, "parts", parts_arr)) { // LCOV_EXCL_BR_LINE
        return false; // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_obj_add_val(doc, root, "systemInstruction", sys_obj)) { // LCOV_EXCL_BR_LINE
        return false; // LCOV_EXCL_LINE
    }

    return true;
}

/**
 * Serialize messages array
 */
static bool serialize_contents(yyjson_mut_doc *doc, yyjson_mut_val *root,
                               const ik_request_t *req, const char *thought_sig)
{
    assert(doc != NULL);  // LCOV_EXCL_BR_LINE
    assert(root != NULL); // LCOV_EXCL_BR_LINE
    assert(req != NULL);  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *contents_arr = yyjson_mut_arr(doc);
    if (!contents_arr) return false; // LCOV_EXCL_BR_LINE

    bool seen_assistant = false;

    for (size_t i = 0; i < req->message_count; i++) {
        const ik_message_t *msg = &req->messages[i]; // LCOV_EXCL_BR_LINE

        yyjson_mut_val *content_obj = yyjson_mut_obj(doc);
        if (!content_obj) return false; // LCOV_EXCL_BR_LINE

        // Add role
        const char *role_str = ik_google_role_to_string(msg->role, req->model);
        if (!yyjson_mut_obj_add_str(doc, content_obj, "role", role_str)) { // LCOV_EXCL_BR_LINE
            return false; // LCOV_EXCL_LINE
        }

        // Determine if this is the first assistant message for thought signature
        bool is_first_assistant = (msg->role == IK_ROLE_ASSISTANT && !seen_assistant);
        if (msg->role == IK_ROLE_ASSISTANT) {
            seen_assistant = true;
        }

        // Add parts
        if (!ik_google_serialize_message_parts(doc, content_obj, msg, thought_sig,
                                                is_first_assistant, req->model,
                                                req->messages, req->message_count, i)) {
            return false;
        }

        if (!yyjson_mut_arr_add_val(contents_arr, content_obj)) { // LCOV_EXCL_BR_LINE
            return false; // LCOV_EXCL_LINE
        }
    }

    if (!yyjson_mut_obj_add_val(doc, root, "contents", contents_arr)) { // LCOV_EXCL_BR_LINE
        return false; // LCOV_EXCL_LINE
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

    if (req->tool_count == 0) {
        return true;
    }

    yyjson_mut_val *tools_arr = yyjson_mut_arr(doc);
    if (!tools_arr) return false; // LCOV_EXCL_BR_LINE

    yyjson_mut_val *tool_obj = yyjson_mut_obj(doc);
    if (!tool_obj) return false; // LCOV_EXCL_BR_LINE

    yyjson_mut_val *func_decls_arr = yyjson_mut_arr(doc);
    if (!func_decls_arr) return false; // LCOV_EXCL_BR_LINE

    for (size_t i = 0; i < req->tool_count; i++) {
        const ik_tool_def_t *tool = &req->tools[i]; // LCOV_EXCL_BR_LINE

        yyjson_mut_val *func_obj = yyjson_mut_obj(doc);
        if (!func_obj) return false; // LCOV_EXCL_BR_LINE

        if (!yyjson_mut_obj_add_str(doc, func_obj, "name", tool->name)) { // LCOV_EXCL_BR_LINE
            return false; // LCOV_EXCL_LINE
        }

        if (!yyjson_mut_obj_add_str(doc, func_obj, "description", tool->description)) { // LCOV_EXCL_BR_LINE
            return false; // LCOV_EXCL_LINE
        }

        // Parse parameters JSON string and add as object
        yyjson_doc *params_doc = yyjson_read(tool->parameters,
                                             strlen(tool->parameters), 0);
        if (!params_doc) return false; // LCOV_EXCL_BR_LINE

        yyjson_mut_val *params_mut = yyjson_val_mut_copy(doc, yyjson_doc_get_root(params_doc));
        yyjson_doc_free(params_doc);
        if (!params_mut) return false; // LCOV_EXCL_BR_LINE

        // Remove additionalProperties - Gemini doesn't support it
        yyjson_mut_obj_remove_key(params_mut, "additionalProperties");

        if (!yyjson_mut_obj_add_val(doc, func_obj, "parameters", params_mut)) { // LCOV_EXCL_BR_LINE
            return false; // LCOV_EXCL_LINE
        }

        if (!yyjson_mut_arr_add_val(func_decls_arr, func_obj)) { // LCOV_EXCL_BR_LINE
            return false; // LCOV_EXCL_LINE
        }
    }

    if (!yyjson_mut_obj_add_val(doc, tool_obj, "functionDeclarations", func_decls_arr)) { // LCOV_EXCL_BR_LINE
        return false; // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_arr_add_val(tools_arr, tool_obj)) { // LCOV_EXCL_BR_LINE
        return false; // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_obj_add_val(doc, root, "tools", tools_arr)) { // LCOV_EXCL_BR_LINE
        return false; // LCOV_EXCL_LINE
    }

    return true;
}

/**
 * Serialize tool config
 */
static bool serialize_tool_config(yyjson_mut_doc *doc, yyjson_mut_val *root,
                                  const ik_request_t *req)
{
    assert(doc != NULL);  // LCOV_EXCL_BR_LINE
    assert(root != NULL); // LCOV_EXCL_BR_LINE
    assert(req != NULL);  // LCOV_EXCL_BR_LINE

    if (req->tool_count == 0) {
        return true;
    }

    // Map tool choice mode
    const char *mode_str = NULL;
    switch (req->tool_choice_mode) {
        case 0: // IK_TOOL_AUTO
            mode_str = "AUTO";
            break;
        case 1: // IK_TOOL_NONE
            mode_str = "NONE";
            break;
        case 2: // IK_TOOL_REQUIRED
            mode_str = "ANY";
            break;
        default:
            mode_str = "AUTO";
            break;
    }

    yyjson_mut_val *tool_config = yyjson_mut_obj(doc);
    if (!tool_config) return false; // LCOV_EXCL_BR_LINE

    yyjson_mut_val *func_config = yyjson_mut_obj(doc);
    if (!func_config) return false; // LCOV_EXCL_BR_LINE

    if (!yyjson_mut_obj_add_str(doc, func_config, "mode", mode_str)) { // LCOV_EXCL_BR_LINE
        return false; // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_obj_add_val(doc, tool_config, "functionCallingConfig", func_config)) { // LCOV_EXCL_BR_LINE
        return false; // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_obj_add_val(doc, root, "toolConfig", tool_config)) { // LCOV_EXCL_BR_LINE
        return false; // LCOV_EXCL_LINE
    }

    return true;
}

/**
 * Serialize generation config (max tokens and thinking)
 */
static bool serialize_generation_config(yyjson_mut_doc *doc, yyjson_mut_val *root,
                                        const ik_request_t *req)
{
    assert(doc != NULL);  // LCOV_EXCL_BR_LINE
    assert(root != NULL); // LCOV_EXCL_BR_LINE
    assert(req != NULL);  // LCOV_EXCL_BR_LINE

    // Check if we need generation config
    bool need_max_tokens = (req->max_output_tokens > 0);
    ik_gemini_series_t series = ik_google_model_series(req->model);
    bool need_thinking;
    if (series == IK_GEMINI_3) {
        // Gemini 3 always sends thinkingConfig (NONE -> "minimal"/"low")
        need_thinking = true;
    } else {
        need_thinking = (req->thinking.level != IK_THINKING_MIN &&
                         ik_google_supports_thinking(req->model));
    }

    if (!need_max_tokens && !need_thinking) {
        return true;
    }

    yyjson_mut_val *gen_config = yyjson_mut_obj(doc);
    if (!gen_config) return false; // LCOV_EXCL_BR_LINE

    // Add max tokens if specified
    if (need_max_tokens) {
        if (!yyjson_mut_obj_add_int(doc, gen_config, "maxOutputTokens", req->max_output_tokens)) { // LCOV_EXCL_BR_LINE
            return false; // LCOV_EXCL_LINE
        }
    }

    // Add thinking config if needed
    if (need_thinking) {
        yyjson_mut_val *thinking_config = yyjson_mut_obj(doc);
        if (!thinking_config) return false; // LCOV_EXCL_BR_LINE

        if (!yyjson_mut_obj_add_bool(doc, thinking_config, "includeThoughts", true)) { // LCOV_EXCL_BR_LINE
            return false; // LCOV_EXCL_LINE
        }

        if (series == IK_GEMINI_2_5) {
            // Gemini 2.5 uses thinking budget
            int32_t budget = ik_google_thinking_budget(req->model, req->thinking.level);
            if (budget >= 0) { // LCOV_EXCL_BR_LINE - budget is always >= 0 for IK_GEMINI_2_5
                if (!yyjson_mut_obj_add_int(doc, thinking_config, "thinkingBudget", budget)) { // LCOV_EXCL_BR_LINE
                    return false; // LCOV_EXCL_LINE
                }
            }
        } else if (series == IK_GEMINI_3) {
            // Gemini 3 uses thinking level (lowercase, per-model mapping)
            const char *level_str = ik_google_thinking_level_str(req->model, req->thinking.level);
            if (!yyjson_mut_obj_add_str(doc, thinking_config, "thinkingLevel", level_str)) { // LCOV_EXCL_BR_LINE
                return false; // LCOV_EXCL_LINE
            }
        }

        if (!yyjson_mut_obj_add_val(doc, gen_config, "thinkingConfig", thinking_config)) { // LCOV_EXCL_BR_LINE
            return false; // LCOV_EXCL_LINE
        }
    }

    if (!yyjson_mut_obj_add_val(doc, root, "generationConfig", gen_config)) { // LCOV_EXCL_BR_LINE
        return false; // LCOV_EXCL_LINE
    }

    return true;
}

/* ================================================================
 * Public API
 * ================================================================ */

res_t ik_google_serialize_request(TALLOC_CTX *ctx, const ik_request_t *req, char **out_json)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(req != NULL);      // LCOV_EXCL_BR_LINE
    assert(out_json != NULL); // LCOV_EXCL_BR_LINE

    if (req->model == NULL) {
        return ERR(ctx, INVALID_ARG, "Model is required");
    }

    // Create JSON document
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (!root) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }
    yyjson_mut_doc_set_root(doc, root);

    // Find latest thought signature (for Gemini 3 models)
    yyjson_doc *sig_doc = NULL;
    const char *thought_sig = ik_google_find_latest_thought_signature(req, &sig_doc);

    // Copy thought signature before freeing doc
    char *thought_sig_copy = NULL;
    if (thought_sig != NULL) {
        thought_sig_copy = talloc_strdup(ctx, thought_sig);
        if (thought_sig_copy == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    // Free thought signature doc if allocated
    if (sig_doc != NULL) {
        yyjson_doc_free(sig_doc); // LCOV_EXCL_BR_LINE - vendor lib internal check
    }

    // Serialize components
    bool success = true;
    success = success && serialize_system_instruction(doc, root, req); // LCOV_EXCL_BR_LINE
    success = success && serialize_contents(doc, root, req, thought_sig_copy); // LCOV_EXCL_BR_LINE
    success = success && serialize_tools(doc, root, req); // LCOV_EXCL_BR_LINE
    success = success && serialize_tool_config(doc, root, req); // LCOV_EXCL_BR_LINE
    success = success && serialize_generation_config(doc, root, req); // LCOV_EXCL_BR_LINE

    if (!success) {
        yyjson_mut_doc_free(doc);
        return ERR(ctx, PARSE, "Failed to serialize request to JSON");
    }

    // Write to string
    size_t len;
    char *json_str = yyjson_mut_write(doc, 0, &len);
    if (!json_str) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        return ERR(ctx, PARSE, "Failed to write JSON to string"); // LCOV_EXCL_LINE
    }

    // Copy to talloc
    char *result = talloc_strdup(ctx, json_str);
    free(json_str); // yyjson uses malloc
    yyjson_mut_doc_free(doc);

    if (!result) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    *out_json = result;
    return OK(result);
}

res_t ik_google_build_url(TALLOC_CTX *ctx, const char *base_url, const char *model,
                          const char *api_key, bool streaming, char **out_url)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(base_url != NULL); // LCOV_EXCL_BR_LINE
    assert(model != NULL);    // LCOV_EXCL_BR_LINE
    assert(api_key != NULL);  // LCOV_EXCL_BR_LINE
    assert(out_url != NULL);  // LCOV_EXCL_BR_LINE

    const char *method = streaming ? "streamGenerateContent" : "generateContent";
    const char *alt_param = streaming ? "&alt=sse" : "";

    char *url = talloc_asprintf(ctx, "%s/models/%s:%s?key=%s%s",
                                base_url, model, method, api_key, alt_param);
    if (!url) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    *out_url = url;
    return OK(url);
}

res_t ik_google_build_headers(TALLOC_CTX *ctx, bool streaming, char ***out_headers)
{
    assert(ctx != NULL);         // LCOV_EXCL_BR_LINE
    assert(out_headers != NULL); // LCOV_EXCL_BR_LINE

    char **headers = talloc_zero_array(ctx, char *, streaming ? 3U : 2U);
    if (!headers) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    headers[0] = talloc_strdup(headers, "Content-Type: application/json");
    if (!headers[0]) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    if (streaming) {
        headers[1] = talloc_strdup(headers, "Accept: text/event-stream");
        if (!headers[1]) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        headers[2] = NULL;
    } else {
        headers[1] = NULL;
    }

    *out_headers = headers;
    return OK(headers);
}
