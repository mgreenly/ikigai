/**
 * @file response_chat.c
 * @brief OpenAI Chat Completions response parsing implementation
 */

#include "response.h"
#include "json_allocator.h"
#include "panic.h"
#include "vendor/yyjson/yyjson.h"
#include <assert.h>
#include <string.h>

/* ================================================================
 * Helper Functions
 * ================================================================ */

/**
 * Parse tool call from JSON (including arguments string parsing)
 */
static res_t parse_chat_tool_call(TALLOC_CTX *ctx, TALLOC_CTX *blocks_ctx, yyjson_val *tc_val,
                                   ik_content_block_t *out_block)
{
    assert(ctx != NULL);        // LCOV_EXCL_BR_LINE
    assert(blocks_ctx != NULL); // LCOV_EXCL_BR_LINE
    assert(tc_val != NULL);     // LCOV_EXCL_BR_LINE
    assert(out_block != NULL);  // LCOV_EXCL_BR_LINE

    out_block->type = IK_CONTENT_TOOL_CALL; // LCOV_EXCL_BR_LINE

    // Extract id
    yyjson_val *id_val = yyjson_obj_get(tc_val, "id");
    if (id_val == NULL) {
        return ERR(ctx, PARSE, "Tool call missing 'id' field");
    }
    const char *id = yyjson_get_str(id_val);
    if (id == NULL) {
        return ERR(ctx, PARSE, "Tool call 'id' is not a string");
    }
    out_block->data.tool_call.id = talloc_strdup(blocks_ctx, id);
    if (out_block->data.tool_call.id == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Extract function object
    yyjson_val *func_val = yyjson_obj_get(tc_val, "function");
    if (func_val == NULL) {
        return ERR(ctx, PARSE, "Tool call missing 'function' field");
    }

    // Extract name
    yyjson_val *name_val = yyjson_obj_get(func_val, "name");
    if (name_val == NULL) {
        return ERR(ctx, PARSE, "Tool call function missing 'name' field");
    }
    const char *name = yyjson_get_str(name_val);
    if (name == NULL) {
        return ERR(ctx, PARSE, "Tool call function 'name' is not a string");
    }
    out_block->data.tool_call.name = talloc_strdup(blocks_ctx, name);
    if (out_block->data.tool_call.name == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Extract arguments (JSON string that needs parsing)
    yyjson_val *args_val = yyjson_obj_get(func_val, "arguments");
    if (args_val == NULL) {
        return ERR(ctx, PARSE, "Tool call function missing 'arguments' field");
    }
    const char *args_str = yyjson_get_str(args_val);
    if (args_str == NULL) {
        return ERR(ctx, PARSE, "Tool call function 'arguments' is not a string");
    }

    // Store the arguments JSON string
    out_block->data.tool_call.arguments = talloc_strdup(blocks_ctx, args_str);
    if (out_block->data.tool_call.arguments == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    return OK(NULL);
}

/**
 * Parse usage statistics from JSON (including reasoning_tokens)
 */
static void parse_chat_usage(yyjson_val *usage_val, ik_usage_t *out_usage)
{
    assert(out_usage != NULL); // LCOV_EXCL_BR_LINE

    // Initialize to zero
    memset(out_usage, 0, sizeof(ik_usage_t));

    if (usage_val == NULL) {
        return; // All zeros
    }

    // Extract prompt_tokens
    yyjson_val *prompt_val = yyjson_obj_get(usage_val, "prompt_tokens");
    if (prompt_val != NULL && yyjson_is_int(prompt_val)) {
        out_usage->input_tokens = (int32_t)yyjson_get_int(prompt_val);
    }

    // Extract completion_tokens
    yyjson_val *completion_val = yyjson_obj_get(usage_val, "completion_tokens");
    if (completion_val != NULL && yyjson_is_int(completion_val)) {
        out_usage->output_tokens = (int32_t)yyjson_get_int(completion_val);
    }

    // Extract total_tokens
    yyjson_val *total_val = yyjson_obj_get(usage_val, "total_tokens");
    if (total_val != NULL && yyjson_is_int(total_val)) {
        out_usage->total_tokens = (int32_t)yyjson_get_int(total_val);
    }

    // Extract reasoning_tokens from completion_tokens_details (optional)
    yyjson_val *details_val = yyjson_obj_get(usage_val, "completion_tokens_details");
    if (details_val != NULL) {
        yyjson_val *reasoning_val = yyjson_obj_get(details_val, "reasoning_tokens");
        if (reasoning_val != NULL && yyjson_is_int(reasoning_val)) {
            out_usage->thinking_tokens = (int32_t)yyjson_get_int(reasoning_val);
        }
    }
}

/* ================================================================
 * Public Functions
 * ================================================================ */

ik_finish_reason_t ik_openai_map_chat_finish_reason(const char *finish_reason)
{
    if (finish_reason == NULL) {
        return IK_FINISH_UNKNOWN;
    }

    if (strcmp(finish_reason, "stop") == 0) {
        return IK_FINISH_STOP;
    } else if (strcmp(finish_reason, "length") == 0) {
        return IK_FINISH_LENGTH;
    } else if (strcmp(finish_reason, "tool_calls") == 0) {
        return IK_FINISH_TOOL_USE;
    } else if (strcmp(finish_reason, "content_filter") == 0) {
        return IK_FINISH_CONTENT_FILTER;
    } else if (strcmp(finish_reason, "error") == 0) {
        return IK_FINISH_ERROR;
    }

    return IK_FINISH_UNKNOWN;
}

res_t ik_openai_parse_chat_response(TALLOC_CTX *ctx, const char *json,
                                     size_t json_len, ik_response_t **out_resp)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(json != NULL);     // LCOV_EXCL_BR_LINE
    assert(out_resp != NULL); // LCOV_EXCL_BR_LINE

    // Parse JSON with talloc allocator
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    // yyjson_read_opts wants non-const pointer but doesn't modify the data (same cast pattern as yyjson.h:993)
    yyjson_doc *doc = yyjson_read_opts((char *)(void *)(size_t)(const void *)json, json_len, 0, &allocator, NULL);
    if (doc == NULL) {
        return ERR(ctx, PARSE, "Invalid JSON response");
    }

    yyjson_val *root = yyjson_doc_get_root(doc); // LCOV_EXCL_BR_LINE
    if (!yyjson_is_obj(root)) {
        return ERR(ctx, PARSE, "Response root is not an object");
    }

    // Check for error response
    yyjson_val *error_obj = yyjson_obj_get(root, "error");
    if (error_obj != NULL) {
        // Extract error message
        const char *error_msg = "Unknown error"; // LCOV_EXCL_BR_LINE
        yyjson_val *msg_val = yyjson_obj_get(error_obj, "message");
        if (msg_val != NULL) {
            const char *msg = yyjson_get_str(msg_val);
            if (msg != NULL) {
                error_msg = msg;
            }
        }
        return ERR(ctx, PROVIDER, "API error: %s", error_msg);
    }

    // Allocate response structure
    ik_response_t *resp = talloc_zero(ctx, ik_response_t);
    if (resp == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Extract model
    yyjson_val *model_val = yyjson_obj_get(root, "model");
    if (model_val != NULL) {
        const char *model = yyjson_get_str(model_val);
        if (model != NULL) {
            resp->model = talloc_strdup(resp, model);
            if (resp->model == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        }
    }

    // Extract usage
    yyjson_val *usage_val = yyjson_obj_get(root, "usage");
    parse_chat_usage(usage_val, &resp->usage);

    // Extract choices array
    yyjson_val *choices_arr = yyjson_obj_get(root, "choices");
    if (choices_arr == NULL || !yyjson_is_arr(choices_arr)) {
        // No choices - empty response
        resp->content_blocks = NULL;
        resp->content_count = 0;
        resp->finish_reason = IK_FINISH_UNKNOWN;
        *out_resp = resp;
        return OK(resp);
    }

    size_t choices_count = yyjson_arr_size(choices_arr);
    if (choices_count == 0) {
        // Empty choices array
        resp->content_blocks = NULL;
        resp->content_count = 0;
        resp->finish_reason = IK_FINISH_UNKNOWN;
        *out_resp = resp;
        return OK(resp);
    }

    // Get first choice (we only use choices[0])
    yyjson_val *choice = yyjson_arr_get_first(choices_arr);
    // LCOV_EXCL_START - defensive: arr_get_first only returns NULL if size==0, already checked
    if (choice == NULL) {
        resp->content_blocks = NULL;
        resp->content_count = 0;
        resp->finish_reason = IK_FINISH_UNKNOWN;
        *out_resp = resp;
        return OK(resp);
    }
    // LCOV_EXCL_STOP

    // Extract finish_reason
    yyjson_val *finish_reason_val = yyjson_obj_get(choice, "finish_reason");
    const char *finish_reason = NULL;
    if (finish_reason_val != NULL) {
        finish_reason = yyjson_get_str(finish_reason_val);
    }
    resp->finish_reason = ik_openai_map_chat_finish_reason(finish_reason); // LCOV_EXCL_BR_LINE

    // Extract message
    yyjson_val *message = yyjson_obj_get(choice, "message");
    if (message == NULL) {
        resp->content_blocks = NULL;
        resp->content_count = 0;
        *out_resp = resp;
        return OK(resp);
    }

    // Count content blocks (text + tool_calls)
    size_t content_count = 0;
    bool has_text = false; // LCOV_EXCL_BR_LINE

    // Check for content field
    yyjson_val *content_val = yyjson_obj_get(message, "content");
    if (content_val != NULL && !yyjson_is_null(content_val)) {
        const char *content_str = yyjson_get_str(content_val);
        if (content_str != NULL && strlen(content_str) > 0) {
            has_text = true;
            content_count++;
        }
    }

    // Check for tool_calls array
    yyjson_val *tool_calls_arr = yyjson_obj_get(message, "tool_calls");
    size_t tool_calls_count = 0;
    if (tool_calls_arr != NULL && yyjson_is_arr(tool_calls_arr)) {
        tool_calls_count = yyjson_arr_size(tool_calls_arr);
        content_count += tool_calls_count;
    }

    if (content_count == 0) {
        resp->content_blocks = NULL;
        resp->content_count = 0;
        *out_resp = resp;
        return OK(resp);
    }

    // Allocate content blocks
    // Safe cast: content block count should never exceed UINT_MAX in practice
    assert(content_count <= (size_t)UINT_MAX); // LCOV_EXCL_BR_LINE
    ik_content_block_t *blocks = talloc_array(resp, ik_content_block_t, (unsigned int)content_count);
    if (blocks == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    size_t block_idx = 0;

    // Add text content if present
    if (has_text) {
        const char *content_str = yyjson_get_str(content_val);
        blocks[block_idx].type = IK_CONTENT_TEXT;
        blocks[block_idx].data.text.text = talloc_strdup(blocks, content_str);
        if (blocks[block_idx].data.text.text == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        block_idx++;
    }

    // Add tool calls if present
    if (tool_calls_count > 0) {
        size_t idx, max;
        yyjson_val *tc_val;
        yyjson_arr_foreach(tool_calls_arr, idx, max, tc_val) { // LCOV_EXCL_BR_LINE - vendor macro loop branches
            res_t result = parse_chat_tool_call(resp, blocks, tc_val, &blocks[block_idx]);
            if (is_err(&result)) {
                return result;
            }
            block_idx++;
        }
    }

    resp->content_blocks = blocks;
    resp->content_count = content_count;
    *out_resp = resp;
    return OK(resp);
}

res_t ik_openai_parse_error(TALLOC_CTX *ctx, int http_status, const char *json,
                             size_t json_len, ik_error_category_t *out_category,
                             char **out_message)
{
    assert(ctx != NULL);          // LCOV_EXCL_BR_LINE
    assert(out_category != NULL); // LCOV_EXCL_BR_LINE
    assert(out_message != NULL);  // LCOV_EXCL_BR_LINE

    // Map HTTP status to category
    switch (http_status) {
        case 400:
            *out_category = IK_ERR_CAT_INVALID_ARG;
            break;
        case 401:
        case 403:
            *out_category = IK_ERR_CAT_AUTH;
            break;
        case 404:
            *out_category = IK_ERR_CAT_NOT_FOUND;
            break;
        case 429:
            *out_category = IK_ERR_CAT_RATE_LIMIT;
            break;
        case 500:
        case 502:
        case 503:
            *out_category = IK_ERR_CAT_SERVER;
            break;
        default:
            *out_category = IK_ERR_CAT_UNKNOWN;
            break;
    }

    // Try to extract error message from JSON
    if (json != NULL && json_len > 0) {
        yyjson_alc allocator = ik_make_talloc_allocator(ctx);
        // yyjson_read_opts wants non-const pointer but doesn't modify the data (same cast pattern as yyjson.h:993)
        yyjson_doc *doc = yyjson_read_opts((char *)(void *)(size_t)(const void *)json, json_len, 0, &allocator, NULL);
        if (doc != NULL) {
            yyjson_val *root = yyjson_doc_get_root(doc); // LCOV_EXCL_BR_LINE
            if (yyjson_is_obj(root)) {
                yyjson_val *error_obj = yyjson_obj_get(root, "error");
                if (error_obj != NULL) {
                    yyjson_val *type_val = yyjson_obj_get(error_obj, "type"); // LCOV_EXCL_BR_LINE
                    yyjson_val *code_val = yyjson_obj_get(error_obj, "code"); // LCOV_EXCL_BR_LINE
                    yyjson_val *msg_val = yyjson_obj_get(error_obj, "message");

                    const char *type_str = NULL;
                    const char *code_str = NULL;
                    const char *msg_str = NULL;

                    if (type_val != NULL) {
                        type_str = yyjson_get_str(type_val);
                    }
                    if (code_val != NULL) {
                        code_str = yyjson_get_str(code_val);
                    }
                    if (msg_val != NULL) {
                        msg_str = yyjson_get_str(msg_val);
                    }

                    // Format: "{type} ({code}): {message}" or "{type}: {message}"
                    if (type_str != NULL && code_str != NULL && msg_str != NULL) {
                        *out_message = talloc_asprintf(ctx, "%s (%s): %s", type_str, code_str, msg_str);
                    } else if (type_str != NULL && msg_str != NULL) {
                        *out_message = talloc_asprintf(ctx, "%s: %s", type_str, msg_str);
                    } else if (msg_str != NULL) {
                        *out_message = talloc_strdup(ctx, msg_str);
                    } else if (type_str != NULL) {
                        *out_message = talloc_strdup(ctx, type_str);
                    } else {
                        *out_message = talloc_asprintf(ctx, "HTTP %d", http_status);
                    }

                    if (*out_message == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
                    return OK(NULL);
                }
            }
        }
    }

    // Fallback to HTTP status message
    *out_message = talloc_asprintf(ctx, "HTTP %d", http_status);
    if (*out_message == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    return OK(NULL);
}
