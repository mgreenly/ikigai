/**
 * @file response.c
 * @brief Anthropic response parsing implementation
 */

#include "response.h"
#include "streaming.h"
#include "request.h"
#include "json_allocator.h"
#include "panic.h"
#include "vendor/yyjson/yyjson.h"
#include <assert.h>
#include <string.h>

/* ================================================================
 * Helper Functions
 * ================================================================ */

/**
 * Parse content block array from JSON
 */
static res_t parse_content_blocks(TALLOC_CTX *ctx, yyjson_val *content_arr,
                                   ik_content_block_t **out_blocks, size_t *out_count)
{
    assert(ctx != NULL);         // LCOV_EXCL_BR_LINE
    assert(content_arr != NULL); // LCOV_EXCL_BR_LINE
    assert(out_blocks != NULL);  // LCOV_EXCL_BR_LINE
    assert(out_count != NULL);   // LCOV_EXCL_BR_LINE

    size_t count = yyjson_arr_size(content_arr);
    if (count == 0) {
        *out_blocks = NULL;
        *out_count = 0;
        return OK(NULL);
    }

    // Safe cast: content block count should never exceed UINT_MAX in practice
    assert(count <= (size_t)UINT_MAX); // LCOV_EXCL_BR_LINE
    ik_content_block_t *blocks = talloc_array(ctx, ik_content_block_t, (unsigned int)count);
    if (blocks == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    size_t idx, max;
    yyjson_val *item;
    yyjson_arr_foreach(content_arr, idx, max, item) {
        yyjson_val *type_val = yyjson_obj_get(item, "type");
        if (type_val == NULL) {
            return ERR(ctx, PARSE, "Content block missing 'type' field");
        }

        const char *type_str = yyjson_get_str(type_val);
        if (type_str == NULL) {
            return ERR(ctx, PARSE, "Content block 'type' is not a string");
        }

        if (strcmp(type_str, "text") == 0) {
            blocks[idx].type = IK_CONTENT_TEXT;
            yyjson_val *text_val = yyjson_obj_get(item, "text");
            if (text_val == NULL) {
                return ERR(ctx, PARSE, "Text block missing 'text' field");
            }
            const char *text = yyjson_get_str(text_val);
            if (text == NULL) {
                return ERR(ctx, PARSE, "Text block 'text' is not a string");
            }
            blocks[idx].data.text.text = talloc_strdup(blocks, text);
            if (blocks[idx].data.text.text == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        } else if (strcmp(type_str, "thinking") == 0) {
            blocks[idx].type = IK_CONTENT_THINKING;
            yyjson_val *thinking_val = yyjson_obj_get(item, "thinking");
            if (thinking_val == NULL) {
                return ERR(ctx, PARSE, "Thinking block missing 'thinking' field");
            }
            const char *thinking = yyjson_get_str(thinking_val);
            if (thinking == NULL) {
                return ERR(ctx, PARSE, "Thinking block 'thinking' is not a string");
            }
            blocks[idx].data.thinking.text = talloc_strdup(blocks, thinking);
            if (blocks[idx].data.thinking.text == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        } else if (strcmp(type_str, "redacted_thinking") == 0) {
            blocks[idx].type = IK_CONTENT_THINKING;
            blocks[idx].data.thinking.text = talloc_strdup(blocks, "[thinking redacted]");
            if (blocks[idx].data.thinking.text == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        } else if (strcmp(type_str, "tool_use") == 0) {
            blocks[idx].type = IK_CONTENT_TOOL_CALL;

            // Extract id
            yyjson_val *id_val = yyjson_obj_get(item, "id");
            if (id_val == NULL) {
                return ERR(ctx, PARSE, "Tool use block missing 'id' field");
            }
            const char *id = yyjson_get_str(id_val);
            if (id == NULL) {
                return ERR(ctx, PARSE, "Tool use 'id' is not a string");
            }
            blocks[idx].data.tool_call.id = talloc_strdup(blocks, id);
            if (blocks[idx].data.tool_call.id == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

            // Extract name
            yyjson_val *name_val = yyjson_obj_get(item, "name");
            if (name_val == NULL) {
                return ERR(ctx, PARSE, "Tool use block missing 'name' field");
            }
            const char *name = yyjson_get_str(name_val);
            if (name == NULL) {
                return ERR(ctx, PARSE, "Tool use 'name' is not a string");
            }
            blocks[idx].data.tool_call.name = talloc_strdup(blocks, name);
            if (blocks[idx].data.tool_call.name == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

            // Extract input (serialize to JSON string)
            yyjson_val *input_val = yyjson_obj_get(item, "input");
            if (input_val == NULL) {
                return ERR(ctx, PARSE, "Tool use block missing 'input' field");
            }

            // Serialize input to JSON string
            char *input_json = yyjson_val_write(input_val, 0, NULL);
            if (input_json == NULL) {
                return ERR(ctx, PARSE, "Failed to serialize tool input");
            }
            blocks[idx].data.tool_call.arguments = talloc_strdup(blocks, input_json);
            free(input_json); // yyjson allocates with malloc
            if (blocks[idx].data.tool_call.arguments == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        } else {
            // Unknown type - log warning and skip (task says to continue parsing)
            // For now, treat as text block with warning marker
            blocks[idx].type = IK_CONTENT_TEXT;
            char *warning = talloc_asprintf(blocks, "[unknown content type: %s]", type_str);
            if (warning == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            blocks[idx].data.text.text = warning;
        }
    }

    *out_blocks = blocks;
    *out_count = count;
    return OK(NULL);
}

/**
 * Parse usage statistics from JSON
 */
static void parse_usage(yyjson_val *usage_obj, ik_usage_t *out_usage)
{
    assert(out_usage != NULL); // LCOV_EXCL_BR_LINE

    // Initialize to zero
    memset(out_usage, 0, sizeof(ik_usage_t));

    if (usage_obj == NULL) {
        return; // All zeros
    }

    // Extract input_tokens
    yyjson_val *input_val = yyjson_obj_get(usage_obj, "input_tokens");
    if (input_val != NULL && yyjson_is_int(input_val)) {
        out_usage->input_tokens = (int32_t)yyjson_get_int(input_val);
    }

    // Extract output_tokens
    yyjson_val *output_val = yyjson_obj_get(usage_obj, "output_tokens");
    if (output_val != NULL && yyjson_is_int(output_val)) {
        out_usage->output_tokens = (int32_t)yyjson_get_int(output_val);
    }

    // Extract thinking_tokens (optional)
    yyjson_val *thinking_val = yyjson_obj_get(usage_obj, "thinking_tokens");
    if (thinking_val != NULL && yyjson_is_int(thinking_val)) {
        out_usage->thinking_tokens = (int32_t)yyjson_get_int(thinking_val);
    }

    // Extract cache_read_input_tokens (optional)
    yyjson_val *cached_val = yyjson_obj_get(usage_obj, "cache_read_input_tokens");
    if (cached_val != NULL && yyjson_is_int(cached_val)) {
        out_usage->cached_tokens = (int32_t)yyjson_get_int(cached_val);
    }

    // Calculate total
    out_usage->total_tokens = out_usage->input_tokens + out_usage->output_tokens +
                              out_usage->thinking_tokens;
}

/* ================================================================
 * Public Functions
 * ================================================================ */

ik_finish_reason_t ik_anthropic_map_finish_reason(const char *stop_reason)
{
    if (stop_reason == NULL) {
        return IK_FINISH_UNKNOWN;
    }

    if (strcmp(stop_reason, "end_turn") == 0) {
        return IK_FINISH_STOP;
    } else if (strcmp(stop_reason, "max_tokens") == 0) {
        return IK_FINISH_LENGTH;
    } else if (strcmp(stop_reason, "tool_use") == 0) {
        return IK_FINISH_TOOL_USE;
    } else if (strcmp(stop_reason, "stop_sequence") == 0) {
        return IK_FINISH_STOP;
    } else if (strcmp(stop_reason, "refusal") == 0) {
        return IK_FINISH_CONTENT_FILTER;
    }

    return IK_FINISH_UNKNOWN;
}

res_t ik_anthropic_parse_response(TALLOC_CTX *ctx, const char *json, size_t json_len,
                                   ik_response_t **out_resp)
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
    yyjson_val *type_val = yyjson_obj_get(root, "type");
    if (type_val != NULL) {
        const char *type_str = yyjson_get_str(type_val);
        if (type_str != NULL && strcmp(type_str, "error") == 0) {
            // Extract error message
            yyjson_val *error_obj = yyjson_obj_get(root, "error");
            const char *error_msg = "Unknown error";
            if (error_obj != NULL) {
                yyjson_val *msg_val = yyjson_obj_get(error_obj, "message");
                if (msg_val != NULL) {
                    const char *msg = yyjson_get_str(msg_val);
                    if (msg != NULL) {
                        error_msg = msg;
                    }
                }
            }
            return ERR(ctx, PROVIDER, "API error: %s", error_msg);
        }
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

    // Extract stop_reason and map to finish_reason
    yyjson_val *stop_reason_val = yyjson_obj_get(root, "stop_reason");
    const char *stop_reason = NULL;
    if (stop_reason_val != NULL) {
        stop_reason = yyjson_get_str(stop_reason_val);
    }
    resp->finish_reason = ik_anthropic_map_finish_reason(stop_reason);

    // Extract usage
    yyjson_val *usage_obj = yyjson_obj_get(root, "usage");
    parse_usage(usage_obj, &resp->usage);

    // Extract content blocks
    yyjson_val *content_arr = yyjson_obj_get(root, "content");
    if (content_arr != NULL && yyjson_is_arr(content_arr)) {
        res_t result = parse_content_blocks(resp, content_arr, &resp->content_blocks,
                                             &resp->content_count);
        if (is_err(&result)) {
            return result;
        }
    } else {
        resp->content_blocks = NULL;
        resp->content_count = 0;
    }

    *out_resp = resp;
    return OK(resp);
}

res_t ik_anthropic_parse_error(TALLOC_CTX *ctx, int http_status, const char *json,
                                size_t json_len, ik_error_category_t *out_category,
                                char **out_message)
{
    assert(ctx != NULL);         // LCOV_EXCL_BR_LINE
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
        case 529:
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
            yyjson_val *root = yyjson_doc_get_root(doc);
            if (yyjson_is_obj(root)) {
                yyjson_val *error_obj = yyjson_obj_get(root, "error");
                if (error_obj != NULL) {
                    yyjson_val *type_val = yyjson_obj_get(error_obj, "type");
                    yyjson_val *msg_val = yyjson_obj_get(error_obj, "message");

                    const char *type_str = NULL;
                    const char *msg_str = NULL;

                    if (type_val != NULL) {
                        type_str = yyjson_get_str(type_val);
                    }
                    if (msg_val != NULL) {
                        msg_str = yyjson_get_str(msg_val);
                    }

                    if (type_str != NULL && msg_str != NULL) {
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

/* ================================================================
 * Async Vtable Implementations (Stubs)
 * ================================================================ */

res_t ik_anthropic_start_request(void *impl_ctx, const ik_request_t *req,
                                  ik_provider_completion_cb_t cb, void *cb_ctx)
{
    assert(impl_ctx != NULL); // LCOV_EXCL_BR_LINE
    assert(req != NULL);      // LCOV_EXCL_BR_LINE
    assert(cb != NULL);       // LCOV_EXCL_BR_LINE

    (void)impl_ctx;
    (void)req;
    (void)cb;
    (void)cb_ctx;

    // Stub: Will be implemented when HTTP multi layer is ready
    // TODO: Serialize request, build headers, start HTTP POST
    return OK(NULL);
}

res_t ik_anthropic_start_stream(void *impl_ctx, const ik_request_t *req,
                                 ik_stream_cb_t stream_cb, void *stream_ctx,
                                 ik_provider_completion_cb_t completion_cb,
                                 void *completion_ctx)
{
    assert(impl_ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(req != NULL);           // LCOV_EXCL_BR_LINE
    assert(stream_cb != NULL);     // LCOV_EXCL_BR_LINE
    assert(completion_cb != NULL); // LCOV_EXCL_BR_LINE

    // TODO: Implement when HTTP multi layer is ready
    // This stub satisfies the vtable requirement but doesn't perform actual streaming yet
    //
    // Implementation plan:
    // 1. Create streaming context with ik_anthropic_stream_ctx_create()
    // 2. Serialize request with ik_anthropic_serialize_request() (stream: true)
    // 3. Build headers with ik_anthropic_build_headers()
    // 4. Construct URL: base_url + "/v1/messages"
    // 5. Configure curl easy handle:
    //    - CURLOPT_URL = constructed URL
    //    - CURLOPT_POST = 1
    //    - CURLOPT_POSTFIELDS = serialized JSON
    //    - CURLOPT_HTTPHEADER = headers
    //    - CURLOPT_WRITEFUNCTION = curl_write_callback (from streaming.c)
    //    - CURLOPT_WRITEDATA = streaming context
    // 6. Add easy handle to curl_multi via ik_http_multi_add_request()
    // 7. Return OK(NULL) immediately

    (void)impl_ctx;
    (void)req;
    (void)stream_cb;
    (void)stream_ctx;
    (void)completion_cb;
    (void)completion_ctx;

    return OK(NULL);
}
