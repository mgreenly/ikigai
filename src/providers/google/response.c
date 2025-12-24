/**
 * @file response.c
 * @brief Google response parsing implementation
 */

#include "response.h"
#include "request.h"
#include "json_allocator.h"
#include "panic.h"
#include "vendor/yyjson/yyjson.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* ================================================================
 * Helper Functions
 * ================================================================ */

/**
 * Generate random 22-character base64url tool call ID
 */
char *ik_google_generate_tool_id(TALLOC_CTX *ctx)
{
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE

    static const char ALPHABET[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    static bool seeded = false;

    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = true;
    }

    char *id = talloc_array(ctx, char, 23); // 22 chars + null
    if (id == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    for (int i = 0; i < 22; i++) {
        id[i] = ALPHABET[rand() % 64];
    }
    id[22] = '\0';

    return id;
}

/**
 * Map Google finishReason to internal finish reason
 */
ik_finish_reason_t ik_google_map_finish_reason(const char *finish_reason)
{
    if (finish_reason == NULL) {
        return IK_FINISH_UNKNOWN;
    }

    if (strcmp(finish_reason, "STOP") == 0) {
        return IK_FINISH_STOP;
    } else if (strcmp(finish_reason, "MAX_TOKENS") == 0) {
        return IK_FINISH_LENGTH;
    } else if (strcmp(finish_reason, "SAFETY") == 0 ||
               strcmp(finish_reason, "BLOCKLIST") == 0 ||
               strcmp(finish_reason, "PROHIBITED_CONTENT") == 0 ||
               strcmp(finish_reason, "IMAGE_SAFETY") == 0 ||
               strcmp(finish_reason, "IMAGE_PROHIBITED_CONTENT") == 0 ||
               strcmp(finish_reason, "RECITATION") == 0) {
        return IK_FINISH_CONTENT_FILTER;
    } else if (strcmp(finish_reason, "MALFORMED_FUNCTION_CALL") == 0 ||
               strcmp(finish_reason, "UNEXPECTED_TOOL_CALL") == 0) {
        return IK_FINISH_ERROR;
    }

    return IK_FINISH_UNKNOWN;
}

/**
 * Parse content parts from candidate
 */
static res_t parse_content_parts(TALLOC_CTX *ctx, yyjson_val *parts_arr,
                                   ik_content_block_t **out_blocks, size_t *out_count)
{
    assert(ctx != NULL);        // LCOV_EXCL_BR_LINE
    assert(parts_arr != NULL);  // LCOV_EXCL_BR_LINE
    assert(out_blocks != NULL); // LCOV_EXCL_BR_LINE
    assert(out_count != NULL);  // LCOV_EXCL_BR_LINE

    size_t count = yyjson_arr_size(parts_arr);
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
    yyjson_val *part;
    yyjson_arr_foreach(parts_arr, idx, max, part) {
        // Check for functionCall (tool call)
        yyjson_val *function_call = yyjson_obj_get(part, "functionCall");
        if (function_call != NULL) {
            blocks[idx].type = IK_CONTENT_TOOL_CALL;

            // Generate tool call ID (Google doesn't provide one)
            blocks[idx].data.tool_call.id = ik_google_generate_tool_id(blocks);

            // Extract function name
            yyjson_val *name_val = yyjson_obj_get(function_call, "name");
            if (name_val == NULL) {
                return ERR(ctx, PARSE, "functionCall missing 'name' field");
            }
            const char *name = yyjson_get_str(name_val);
            if (name == NULL) {
                return ERR(ctx, PARSE, "functionCall 'name' is not a string");
            }
            blocks[idx].data.tool_call.name = talloc_strdup(blocks, name);
            if (blocks[idx].data.tool_call.name == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

            // Extract arguments (serialize to JSON string)
            yyjson_val *args_val = yyjson_obj_get(function_call, "args");
            if (args_val != NULL) {
                // Serialize args object to JSON string
                yyjson_write_flag flg = YYJSON_WRITE_NOFLAG;
                size_t json_len;
                char *args_json = yyjson_val_write_opts(args_val, flg, NULL, &json_len, NULL);
                if (args_json == NULL) {
                    return ERR(ctx, PARSE, "Failed to serialize functionCall args");
                }
                blocks[idx].data.tool_call.arguments = talloc_strdup(blocks, args_json);
                free(args_json); // yyjson uses malloc
                if (blocks[idx].data.tool_call.arguments == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            } else {
                // No args, use empty object
                blocks[idx].data.tool_call.arguments = talloc_strdup(blocks, "{}");
                if (blocks[idx].data.tool_call.arguments == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            }
            continue;
        }

        // Check for thought flag (thinking)
        yyjson_val *thought_val = yyjson_obj_get(part, "thought");
        bool is_thought = thought_val != NULL && yyjson_get_bool(thought_val);

        // Extract text
        yyjson_val *text_val = yyjson_obj_get(part, "text");
        if (text_val == NULL) {
            // Skip parts without text or functionCall
            continue;
        }
        const char *text = yyjson_get_str(text_val);
        if (text == NULL) {
            return ERR(ctx, PARSE, "Part 'text' is not a string");
        }

        if (is_thought) {
            blocks[idx].type = IK_CONTENT_THINKING;
            blocks[idx].data.thinking.text = talloc_strdup(blocks, text);
            if (blocks[idx].data.thinking.text == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        } else {
            blocks[idx].type = IK_CONTENT_TEXT;
            blocks[idx].data.text.text = talloc_strdup(blocks, text);
            if (blocks[idx].data.text.text == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        }
    }

    *out_blocks = blocks;
    *out_count = count;
    return OK(NULL);
}

/**
 * Extract thought signature from response (Gemini 3 only)
 */
static char *extract_thought_signature(TALLOC_CTX *ctx, yyjson_val *root)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(root != NULL); // LCOV_EXCL_BR_LINE

    // Try to find thoughtSignature in various possible locations
    // Location varies by API version and model
    yyjson_val *sig = yyjson_obj_get(root, "thoughtSignature");
    if (sig == NULL) {
        // Try in candidates[0]
        yyjson_val *candidates = yyjson_obj_get(root, "candidates");
        if (candidates != NULL && yyjson_is_arr(candidates)) {
            yyjson_val *first = yyjson_arr_get_first(candidates);
            if (first != NULL) {
                sig = yyjson_obj_get(first, "thoughtSignature");
            }
        }
    }

    if (sig == NULL || !yyjson_is_str(sig)) {
        return NULL; // No signature found
    }

    const char *sig_str = yyjson_get_str(sig);
    if (sig_str == NULL || sig_str[0] == '\0') {
        return NULL;
    }

    // Build provider_data JSON: {"thought_signature": "value"}
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (doc == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    if (obj == NULL) {
        yyjson_mut_doc_free(doc);
        PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    yyjson_mut_val *key = yyjson_mut_strcpy(doc, "thought_signature");
    yyjson_mut_val *val = yyjson_mut_strcpy(doc, sig_str);
    if (key == NULL || val == NULL) {
        yyjson_mut_doc_free(doc);
        PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    yyjson_mut_obj_add(obj, key, val);
    yyjson_mut_doc_set_root(doc, obj);

    // Serialize to string
    size_t json_len;
    char *json = yyjson_mut_write(doc, YYJSON_WRITE_NOFLAG, &json_len);
    yyjson_mut_doc_free(doc);

    if (json == NULL) {
        return NULL; // Failed to serialize
    }

    char *result = talloc_strdup(ctx, json);
    free(json); // yyjson uses malloc

    if (result == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    return result;
}

/**
 * Parse Google JSON response to internal format
 */
res_t ik_google_parse_response(TALLOC_CTX *ctx, const char *json, size_t json_len,
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
        yyjson_doc_free(doc);
        return ERR(ctx, PARSE, "Root is not an object");
    }

    // Check for error response
    yyjson_val *error_obj = yyjson_obj_get(root, "error");
    if (error_obj != NULL) {
        yyjson_val *msg_val = yyjson_obj_get(error_obj, "message");
        const char *msg = msg_val ? yyjson_get_str(msg_val) : "Unknown error";
        yyjson_doc_free(doc);
        return ERR(ctx, PROVIDER, "API error: %s", msg ? msg : "Unknown error");
    }

    // Check for blocked prompt
    yyjson_val *feedback = yyjson_obj_get(root, "promptFeedback");
    if (feedback != NULL) {
        yyjson_val *block_reason = yyjson_obj_get(feedback, "blockReason");
        if (block_reason != NULL) {
            const char *reason = yyjson_get_str(block_reason);
            yyjson_doc_free(doc);
            return ERR(ctx, PROVIDER, "Prompt blocked: %s", reason ? reason : "Unknown reason");
        }
    }

    // Allocate response
    ik_response_t *resp = talloc_zero(ctx, ik_response_t);
    if (resp == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Extract model version
    yyjson_val *model_val = yyjson_obj_get(root, "modelVersion");
    if (model_val != NULL) {
        const char *model = yyjson_get_str(model_val);
        if (model != NULL) {
            resp->model = talloc_strdup(resp, model);
            if (resp->model == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        }
    }

    // Extract usage metadata
    yyjson_val *usage = yyjson_obj_get(root, "usageMetadata");
    if (usage != NULL) {
        yyjson_val *prompt_tokens = yyjson_obj_get(usage, "promptTokenCount");
        yyjson_val *candidates_tokens = yyjson_obj_get(usage, "candidatesTokenCount");
        yyjson_val *thoughts_tokens = yyjson_obj_get(usage, "thoughtsTokenCount");
        yyjson_val *total_tokens = yyjson_obj_get(usage, "totalTokenCount");

        int32_t prompt = prompt_tokens ? (int32_t)yyjson_get_int(prompt_tokens) : 0;
        int32_t candidates = candidates_tokens ? (int32_t)yyjson_get_int(candidates_tokens) : 0;
        int32_t thoughts = thoughts_tokens ? (int32_t)yyjson_get_int(thoughts_tokens) : 0;

        resp->usage.input_tokens = prompt;
        resp->usage.thinking_tokens = thoughts;
        resp->usage.output_tokens = candidates - thoughts; // Exclude thinking from output
        resp->usage.total_tokens = total_tokens ? (int32_t)yyjson_get_int(total_tokens) : 0;
        resp->usage.cached_tokens = 0; // Google doesn't report cache hits
    }

    // Extract candidates array
    yyjson_val *candidates = yyjson_obj_get(root, "candidates");
    if (candidates == NULL || !yyjson_is_arr(candidates)) {
        // No candidates, return empty response
        resp->content_blocks = NULL;
        resp->content_count = 0;
        resp->finish_reason = IK_FINISH_UNKNOWN;
        resp->provider_data = NULL;
        *out_resp = resp;
        yyjson_doc_free(doc);
        return OK(resp);
    }

    // Get first candidate
    yyjson_val *candidate = yyjson_arr_get_first(candidates);
    if (candidate == NULL) {
        // Empty candidates array
        resp->content_blocks = NULL;
        resp->content_count = 0;
        resp->finish_reason = IK_FINISH_UNKNOWN;
        resp->provider_data = NULL;
        *out_resp = resp;
        yyjson_doc_free(doc);
        return OK(resp);
    }

    // Extract finish reason
    yyjson_val *finish_val = yyjson_obj_get(candidate, "finishReason");
    const char *finish_str = finish_val ? yyjson_get_str(finish_val) : NULL;
    resp->finish_reason = ik_google_map_finish_reason(finish_str);

    // Extract content parts
    yyjson_val *content = yyjson_obj_get(candidate, "content");
    if (content != NULL) {
        yyjson_val *parts = yyjson_obj_get(content, "parts");
        if (parts != NULL && yyjson_is_arr(parts)) {
            res_t result = parse_content_parts(resp, parts, &resp->content_blocks,
                                                &resp->content_count);
            if (is_err(&result)) {
                yyjson_doc_free(doc);
                return result;
            }
        } else {
            resp->content_blocks = NULL;
            resp->content_count = 0;
        }
    } else {
        resp->content_blocks = NULL;
        resp->content_count = 0;
    }

    // Extract thought signature (Gemini 3 only)
    resp->provider_data = extract_thought_signature(resp, root);

    *out_resp = resp;
    yyjson_doc_free(doc);
    return OK(resp);
}

/**
 * Parse Google error response
 */
res_t ik_google_parse_error(TALLOC_CTX *ctx, int http_status, const char *json,
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
        case 504:
            *out_category = IK_ERR_CAT_TIMEOUT;
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
                    yyjson_val *msg_val = yyjson_obj_get(error_obj, "message");
                    const char *msg = yyjson_get_str(msg_val);
                    if (msg != NULL) {
                        *out_message = talloc_asprintf(ctx, "%d: %s", http_status, msg);
                        yyjson_doc_free(doc);
                        if (*out_message == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
                        return OK(NULL);
                    }
                }
            }
            yyjson_doc_free(doc);
        }
    }

    // Fallback to generic message
    *out_message = talloc_asprintf(ctx, "HTTP %d", http_status);
    if (*out_message == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    return OK(NULL);
}

/* ================================================================
 * Vtable Implementations (Stubs for Future Tasks)
 * ================================================================ */

res_t ik_google_start_request(void *impl_ctx, const ik_request_t *req,
                                ik_provider_completion_cb_t cb, void *cb_ctx)
{
    assert(impl_ctx != NULL); // LCOV_EXCL_BR_LINE
    assert(req != NULL);      // LCOV_EXCL_BR_LINE
    assert(cb != NULL);       // LCOV_EXCL_BR_LINE

    (void)impl_ctx;
    (void)req;
    (void)cb;
    (void)cb_ctx;

    // Stub: Will be implemented in google-http.md task
    return OK(NULL);
}

res_t ik_google_start_stream(void *impl_ctx, const ik_request_t *req,
                               ik_stream_cb_t stream_cb, void *stream_ctx,
                               ik_provider_completion_cb_t completion_cb,
                               void *completion_ctx)
{
    assert(impl_ctx != NULL);     // LCOV_EXCL_BR_LINE
    assert(req != NULL);          // LCOV_EXCL_BR_LINE
    assert(stream_cb != NULL);    // LCOV_EXCL_BR_LINE
    assert(completion_cb != NULL); // LCOV_EXCL_BR_LINE

    (void)impl_ctx;
    (void)req;
    (void)stream_cb;
    (void)stream_ctx;
    (void)completion_cb;
    (void)completion_ctx;

    // Stub: Will be implemented in google-streaming.md task
    return OK(NULL);
}
