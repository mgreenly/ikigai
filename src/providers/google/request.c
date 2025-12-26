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

#include "request.h"
#include "thinking.h"
#include "error.h"
#include "panic.h"
#include "vendor/yyjson/yyjson.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>

/* ================================================================
 * Helper Functions
 * ================================================================ */

/**
 * Map internal role to Google role string
 */
static const char *role_to_string(ik_role_t role)
{
    switch (role) {
        case IK_ROLE_USER:
            return "user";
        case IK_ROLE_ASSISTANT:
            return "model";
        case IK_ROLE_TOOL:
            return "function";
        default: // LCOV_EXCL_LINE
            return "user"; // LCOV_EXCL_LINE
    }
}

/**
 * Serialize a single content block to Google JSON format
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
            if (!yyjson_mut_obj_add_str(doc, obj, "text", block->data.text.text)) {
                return false; // LCOV_EXCL_BR_LINE
            }
            break;

        case IK_CONTENT_THINKING:
            if (!yyjson_mut_obj_add_str(doc, obj, "text", block->data.thinking.text)) {
                return false; // LCOV_EXCL_BR_LINE
            }
            if (!yyjson_mut_obj_add_bool(doc, obj, "thought", true)) {
                return false; // LCOV_EXCL_BR_LINE
            }
            break;

        case IK_CONTENT_TOOL_CALL: {
            // Build functionCall object
            yyjson_mut_val *func_obj = yyjson_mut_obj(doc);
            if (!func_obj) return false; // LCOV_EXCL_BR_LINE

            if (!yyjson_mut_obj_add_str(doc, func_obj, "name", block->data.tool_call.name)) {
                return false; // LCOV_EXCL_BR_LINE
            }

            // Parse arguments JSON string and add as object
            yyjson_doc *args_doc = yyjson_read(block->data.tool_call.arguments,
                                               strlen(block->data.tool_call.arguments), 0);
            if (!args_doc) return false; // LCOV_EXCL_BR_LINE

            yyjson_mut_val *args_mut = yyjson_val_mut_copy(doc, yyjson_doc_get_root(args_doc));
            yyjson_doc_free(args_doc);
            if (!args_mut) return false; // LCOV_EXCL_BR_LINE

            if (!yyjson_mut_obj_add_val(doc, func_obj, "args", args_mut)) {
                return false; // LCOV_EXCL_BR_LINE
            }

            if (!yyjson_mut_obj_add_val(doc, obj, "functionCall", func_obj)) {
                return false; // LCOV_EXCL_BR_LINE
            }
            break;
        }

        case IK_CONTENT_TOOL_RESULT: {
            // Build functionResponse object
            yyjson_mut_val *func_resp = yyjson_mut_obj(doc);
            if (!func_resp) return false; // LCOV_EXCL_BR_LINE

            if (!yyjson_mut_obj_add_str(doc, func_resp, "name", block->data.tool_result.tool_call_id)) {
                return false; // LCOV_EXCL_BR_LINE
            }

            // Build response object with content field
            yyjson_mut_val *response_obj = yyjson_mut_obj(doc);
            if (!response_obj) return false; // LCOV_EXCL_BR_LINE

            if (!yyjson_mut_obj_add_str(doc, response_obj, "content", block->data.tool_result.content)) {
                return false; // LCOV_EXCL_BR_LINE
            }

            if (!yyjson_mut_obj_add_val(doc, func_resp, "response", response_obj)) {
                return false; // LCOV_EXCL_BR_LINE
            }

            if (!yyjson_mut_obj_add_val(doc, obj, "functionResponse", func_resp)) {
                return false; // LCOV_EXCL_BR_LINE
            }
            break;
        }

        default: // LCOV_EXCL_LINE
            return false; // LCOV_EXCL_LINE
    }

    if (!yyjson_mut_arr_add_val(arr, obj)) return false; // LCOV_EXCL_BR_LINE
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
static const char *extract_thought_signature(const char *metadata, yyjson_doc **out_doc)
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
    if (!root || !yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return NULL;
    }

    yyjson_val *sig = yyjson_obj_get(root, "thought_signature");
    if (!sig || !yyjson_is_str(sig)) {
        yyjson_doc_free(doc);
        return NULL;
    }

    const char *sig_str = yyjson_get_str(sig);
    if (sig_str == NULL || sig_str[0] == '\0') {
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
static const char *find_latest_thought_signature(const ik_request_t *req, yyjson_doc **out_doc)
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

        const char *sig = extract_thought_signature(msg->provider_metadata, out_doc);
        if (sig != NULL) {
            return sig;
        }
    }

    return NULL;
}

/**
 * Serialize message parts array
 */
static bool serialize_message_parts(yyjson_mut_doc *doc, yyjson_mut_val *content_obj,
                                      const ik_message_t *message, const char *thought_sig,
                                      bool is_first_assistant)
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

        if (!yyjson_mut_obj_add_str(doc, sig_obj, "thoughtSignature", thought_sig)) {
            return false; // LCOV_EXCL_BR_LINE
        }

        if (!yyjson_mut_arr_add_val(parts_arr, sig_obj)) {
            return false; // LCOV_EXCL_BR_LINE
        }
    }

    // Add regular content blocks
    for (size_t i = 0; i < message->content_count; i++) {
        if (!serialize_content_block(doc, parts_arr, &message->content_blocks[i])) {
            return false;
        }
    }

    if (!yyjson_mut_obj_add_val(doc, content_obj, "parts", parts_arr)) {
        return false; // LCOV_EXCL_BR_LINE
    }

    return true;
}

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

    if (!yyjson_mut_obj_add_str(doc, part_obj, "text", req->system_prompt)) {
        return false; // LCOV_EXCL_BR_LINE
    }

    if (!yyjson_mut_arr_add_val(parts_arr, part_obj)) {
        return false; // LCOV_EXCL_BR_LINE
    }

    if (!yyjson_mut_obj_add_val(doc, sys_obj, "parts", parts_arr)) {
        return false; // LCOV_EXCL_BR_LINE
    }

    if (!yyjson_mut_obj_add_val(doc, root, "systemInstruction", sys_obj)) {
        return false; // LCOV_EXCL_BR_LINE
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
        const ik_message_t *msg = &req->messages[i];

        yyjson_mut_val *content_obj = yyjson_mut_obj(doc);
        if (!content_obj) return false; // LCOV_EXCL_BR_LINE

        // Add role
        const char *role_str = role_to_string(msg->role);
        if (!yyjson_mut_obj_add_str(doc, content_obj, "role", role_str)) {
            return false; // LCOV_EXCL_BR_LINE
        }

        // Determine if this is the first assistant message for thought signature
        bool is_first_assistant = (msg->role == IK_ROLE_ASSISTANT && !seen_assistant);
        if (msg->role == IK_ROLE_ASSISTANT) {
            seen_assistant = true;
        }

        // Add parts
        if (!serialize_message_parts(doc, content_obj, msg, thought_sig, is_first_assistant)) {
            return false;
        }

        if (!yyjson_mut_arr_add_val(contents_arr, content_obj)) {
            return false; // LCOV_EXCL_BR_LINE
        }
    }

    if (!yyjson_mut_obj_add_val(doc, root, "contents", contents_arr)) {
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
        const ik_tool_def_t *tool = &req->tools[i];

        yyjson_mut_val *func_obj = yyjson_mut_obj(doc);
        if (!func_obj) return false; // LCOV_EXCL_BR_LINE

        if (!yyjson_mut_obj_add_str(doc, func_obj, "name", tool->name)) {
            return false; // LCOV_EXCL_BR_LINE
        }

        if (!yyjson_mut_obj_add_str(doc, func_obj, "description", tool->description)) {
            return false; // LCOV_EXCL_BR_LINE
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

        if (!yyjson_mut_obj_add_val(doc, func_obj, "parameters", params_mut)) {
            return false; // LCOV_EXCL_BR_LINE
        }

        if (!yyjson_mut_arr_add_val(func_decls_arr, func_obj)) {
            return false; // LCOV_EXCL_BR_LINE
        }
    }

    if (!yyjson_mut_obj_add_val(doc, tool_obj, "functionDeclarations", func_decls_arr)) {
        return false; // LCOV_EXCL_BR_LINE
    }

    if (!yyjson_mut_arr_add_val(tools_arr, tool_obj)) {
        return false; // LCOV_EXCL_BR_LINE
    }

    if (!yyjson_mut_obj_add_val(doc, root, "tools", tools_arr)) {
        return false; // LCOV_EXCL_BR_LINE
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

    if (!yyjson_mut_obj_add_str(doc, func_config, "mode", mode_str)) {
        return false; // LCOV_EXCL_BR_LINE
    }

    if (!yyjson_mut_obj_add_val(doc, tool_config, "functionCallingConfig", func_config)) {
        return false; // LCOV_EXCL_BR_LINE
    }

    if (!yyjson_mut_obj_add_val(doc, root, "toolConfig", tool_config)) {
        return false; // LCOV_EXCL_BR_LINE
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
    bool need_thinking = (req->thinking.level != IK_THINKING_NONE &&
                          ik_google_supports_thinking(req->model));

    if (!need_max_tokens && !need_thinking) {
        return true;
    }

    yyjson_mut_val *gen_config = yyjson_mut_obj(doc);
    if (!gen_config) return false; // LCOV_EXCL_BR_LINE

    // Add max tokens if specified
    if (need_max_tokens) {
        if (!yyjson_mut_obj_add_int(doc, gen_config, "maxOutputTokens", req->max_output_tokens)) {
            return false; // LCOV_EXCL_BR_LINE
        }
    }

    // Add thinking config if needed
    if (need_thinking) {
        yyjson_mut_val *thinking_config = yyjson_mut_obj(doc);
        if (!thinking_config) return false; // LCOV_EXCL_BR_LINE

        if (!yyjson_mut_obj_add_bool(doc, thinking_config, "includeThoughts", true)) {
            return false; // LCOV_EXCL_BR_LINE
        }

        ik_gemini_series_t series = ik_google_model_series(req->model);
        if (series == IK_GEMINI_2_5) {
            // Gemini 2.5 uses thinking budget
            int32_t budget = ik_google_thinking_budget(req->model, req->thinking.level);
            if (budget >= 0) {
                if (!yyjson_mut_obj_add_int(doc, thinking_config, "thinkingBudget", budget)) {
                    return false; // LCOV_EXCL_BR_LINE
                }
            }
        } else if (series == IK_GEMINI_3) {
            // Gemini 3 uses thinking level
            const char *level_str = ik_google_thinking_level_str(req->thinking.level);
            if (level_str != NULL) {
                if (!yyjson_mut_obj_add_str(doc, thinking_config, "thinkingLevel", level_str)) {
                    return false; // LCOV_EXCL_BR_LINE
                }
            }
        }

        if (!yyjson_mut_obj_add_val(doc, gen_config, "thinkingConfig", thinking_config)) {
            return false; // LCOV_EXCL_BR_LINE
        }
    }

    if (!yyjson_mut_obj_add_val(doc, root, "generationConfig", gen_config)) {
        return false; // LCOV_EXCL_BR_LINE
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
    const char *thought_sig = find_latest_thought_signature(req, &sig_doc);

    // Serialize components
    bool success = true;
    success = success && serialize_system_instruction(doc, root, req);
    success = success && serialize_contents(doc, root, req, thought_sig);
    success = success && serialize_tools(doc, root, req);
    success = success && serialize_tool_config(doc, root, req);
    success = success && serialize_generation_config(doc, root, req);

    // Free thought signature doc if allocated
    if (sig_doc != NULL) {
        yyjson_doc_free(sig_doc);
    }

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

    char **headers = talloc_array(ctx, char *, streaming ? 3U : 2U);
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
