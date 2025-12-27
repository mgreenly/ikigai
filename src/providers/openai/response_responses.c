/**
 * @file response_responses.c
 * @brief OpenAI Responses API response parsing implementation
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
 * Parse usage statistics from JSON (including reasoning_tokens)
 */
static void parse_usage(yyjson_val *usage_val, ik_usage_t *out_usage)
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

/**
 * Parse function call output item
 */
static res_t parse_function_call(TALLOC_CTX *ctx, TALLOC_CTX *blocks_ctx,
                                  yyjson_val *item, ik_content_block_t *out_block)
{
    assert(ctx != NULL);        // LCOV_EXCL_BR_LINE
    assert(blocks_ctx != NULL); // LCOV_EXCL_BR_LINE
    assert(item != NULL);       // LCOV_EXCL_BR_LINE
    assert(out_block != NULL);  // LCOV_EXCL_BR_LINE

    out_block->type = IK_CONTENT_TOOL_CALL;

    // Extract id (required)
    yyjson_val *id_val = yyjson_obj_get(item, "id");
    const char *id = NULL;
    if (id_val != NULL) {
        id = yyjson_get_str(id_val);
    }

    // Extract call_id (prefer over id)
    yyjson_val *call_id_val = yyjson_obj_get(item, "call_id");
    if (call_id_val != NULL) {
        const char *call_id = yyjson_get_str(call_id_val);
        if (call_id != NULL) {
            id = call_id; // Prefer call_id
        }
    }

    if (id == NULL) {
        return ERR(ctx, PARSE, "Function call missing 'id' or 'call_id' field");
    }

    out_block->data.tool_call.id = talloc_strdup(blocks_ctx, id);
    if (out_block->data.tool_call.id == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Extract name
    yyjson_val *name_val = yyjson_obj_get(item, "name");
    if (name_val == NULL) {
        return ERR(ctx, PARSE, "Function call missing 'name' field");
    }
    const char *name = yyjson_get_str(name_val);
    if (name == NULL) {
        return ERR(ctx, PARSE, "Function call 'name' is not a string");
    }
    out_block->data.tool_call.name = talloc_strdup(blocks_ctx, name);
    if (out_block->data.tool_call.name == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Extract arguments (JSON string)
    yyjson_val *args_val = yyjson_obj_get(item, "arguments");
    if (args_val == NULL) {
        return ERR(ctx, PARSE, "Function call missing 'arguments' field");
    }
    const char *args_str = yyjson_get_str(args_val);
    if (args_str == NULL) {
        return ERR(ctx, PARSE, "Function call 'arguments' is not a string");
    }

    // Store the arguments JSON string
    out_block->data.tool_call.arguments = talloc_strdup(blocks_ctx, args_str);
    if (out_block->data.tool_call.arguments == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    return OK(NULL);
}

/**
 * Count total content blocks in output array
 */
static size_t count_content_blocks(yyjson_val *output_arr)
{
    assert(output_arr != NULL); // LCOV_EXCL_BR_LINE

    if (!yyjson_is_arr(output_arr)) { // LCOV_EXCL_LINE - defensive: caller validates type
        return 0; // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    size_t total = 0;
    size_t idx, max;
    yyjson_val *item;

    yyjson_arr_foreach(output_arr, idx, max, item) {
        // Get type field
        yyjson_val *type_val = yyjson_obj_get(item, "type");
        if (type_val == NULL) {
            continue;
        }
        const char *type = yyjson_get_str(type_val);
        if (type == NULL) {
            continue;
        }

        if (strcmp(type, "message") == 0) {
            // Count content blocks in message
            yyjson_val *content_arr = yyjson_obj_get(item, "content");
            if (content_arr != NULL && yyjson_is_arr(content_arr)) {
                total += yyjson_arr_size(content_arr);
            }
        } else if (strcmp(type, "function_call") == 0) {
            // Function call is one block
            total++;
        }
    }

    return total;
}

/* ================================================================
 * Public Functions
 * ================================================================ */

ik_finish_reason_t ik_openai_map_responses_status(const char *status, const char *incomplete_reason)
{
    if (status == NULL) {
        return IK_FINISH_UNKNOWN;
    }

    if (strcmp(status, "completed") == 0) {
        return IK_FINISH_STOP;
    } else if (strcmp(status, "failed") == 0) {
        return IK_FINISH_ERROR;
    } else if (strcmp(status, "cancelled") == 0) {
        return IK_FINISH_STOP;
    } else if (strcmp(status, "incomplete") == 0) {
        // Check incomplete_details.reason
        if (incomplete_reason != NULL) {
            if (strcmp(incomplete_reason, "max_output_tokens") == 0) {
                return IK_FINISH_LENGTH;
            } else if (strcmp(incomplete_reason, "content_filter") == 0) {
                return IK_FINISH_CONTENT_FILTER;
            }
        }
        // Default incomplete to length
        return IK_FINISH_LENGTH;
    }

    return IK_FINISH_UNKNOWN;
}

res_t ik_openai_parse_responses_response(TALLOC_CTX *ctx, const char *json,
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

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        return ERR(ctx, PARSE, "Response root is not an object");
    }

    // Check for error response
    yyjson_val *error_obj = yyjson_obj_get(root, "error");
    if (error_obj != NULL) {
        // Extract error message
        const char *error_msg = "Unknown error";
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
    parse_usage(usage_val, &resp->usage);

    // Extract status and incomplete_details
    yyjson_val *status_val = yyjson_obj_get(root, "status");
    const char *status = NULL;
    if (status_val != NULL) {
        status = yyjson_get_str(status_val);
    }

    const char *incomplete_reason = NULL;
    yyjson_val *incomplete_details = yyjson_obj_get(root, "incomplete_details");
    if (incomplete_details != NULL) {
        yyjson_val *reason_val = yyjson_obj_get(incomplete_details, "reason");
        if (reason_val != NULL) {
            incomplete_reason = yyjson_get_str(reason_val);
        }
    }

    resp->finish_reason = ik_openai_map_responses_status(status, incomplete_reason);

    // Extract output array
    yyjson_val *output_arr = yyjson_obj_get(root, "output");
    if (output_arr == NULL || !yyjson_is_arr(output_arr)) {
        // No output - empty response
        resp->content_blocks = NULL;
        resp->content_count = 0;
        *out_resp = resp;
        return OK(resp);
    }

    size_t output_count = yyjson_arr_size(output_arr);
    if (output_count == 0) {
        // Empty output array
        resp->content_blocks = NULL;
        resp->content_count = 0;
        *out_resp = resp;
        return OK(resp);
    }

    // Count total content blocks
    size_t content_count = count_content_blocks(output_arr);
    if (content_count == 0) {
        resp->content_blocks = NULL;
        resp->content_count = 0;
        *out_resp = resp;
        return OK(resp);
    }

    // Allocate content blocks
    assert(content_count <= (size_t)UINT_MAX); // LCOV_EXCL_BR_LINE
    ik_content_block_t *blocks = talloc_array(resp, ik_content_block_t, (unsigned int)content_count);
    if (blocks == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    size_t block_idx = 0;

    // Process output array
    size_t out_idx, out_max;
    yyjson_val *item;
    yyjson_arr_foreach(output_arr, out_idx, out_max, item) {
        // Get type field
        yyjson_val *type_val = yyjson_obj_get(item, "type");
        if (type_val == NULL) {
            continue;
        }
        const char *type = yyjson_get_str(type_val);
        if (type == NULL) {
            continue;
        }

        if (strcmp(type, "message") == 0) {
            // Process message content array
            yyjson_val *content_arr = yyjson_obj_get(item, "content");
            if (content_arr == NULL || !yyjson_is_arr(content_arr)) {
                continue;
            }

            size_t content_idx, content_max;
            yyjson_val *content_item;
            yyjson_arr_foreach(content_arr, content_idx, content_max, content_item) {
                // Get content type
                yyjson_val *content_type_val = yyjson_obj_get(content_item, "type");
                if (content_type_val == NULL) {
                    continue;
                }
                const char *content_type = yyjson_get_str(content_type_val);
                if (content_type == NULL) {
                    continue;
                }

                if (strcmp(content_type, "output_text") == 0) {
                    // Extract text from output_text
                    yyjson_val *text_val = yyjson_obj_get(content_item, "text");
                    if (text_val != NULL) {
                        const char *text = yyjson_get_str(text_val);
                        if (text != NULL) {
                            blocks[block_idx].type = IK_CONTENT_TEXT;
                            blocks[block_idx].data.text.text = talloc_strdup(blocks, text);
                            if (blocks[block_idx].data.text.text == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
                            block_idx++;
                        }
                    }
                } else if (strcmp(content_type, "refusal") == 0) {
                    // Extract refusal text
                    yyjson_val *refusal_val = yyjson_obj_get(content_item, "refusal");
                    if (refusal_val != NULL) {
                        const char *refusal = yyjson_get_str(refusal_val);
                        if (refusal != NULL) {
                            blocks[block_idx].type = IK_CONTENT_TEXT;
                            blocks[block_idx].data.text.text = talloc_strdup(blocks, refusal);
                            if (blocks[block_idx].data.text.text == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
                            block_idx++;
                        }
                    }
                }
            }
        } else if (strcmp(type, "function_call") == 0) {
            // Parse function call
            res_t result = parse_function_call(resp, blocks, item, &blocks[block_idx]);
            if (is_err(&result)) {
                return result;
            }
            block_idx++;
        }
    }

    // Update content_count to actual blocks added (may be less if some items were skipped)
    resp->content_blocks = blocks;
    resp->content_count = block_idx;
    *out_resp = resp;
    return OK(resp);
}
