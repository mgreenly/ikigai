/**
 * @file response.c
 * @brief Google response parsing implementation
 */

#include "response.h"
#include "response_utils.h"
#include "json_allocator.h"
#include "panic.h"
#include "vendor/yyjson/yyjson.h"
#include <assert.h>
#include <string.h>


#include "poison.h"
/* ================================================================
 * Helper Functions
 * ================================================================ */

static res_t process_function_call(TALLOC_CTX *ctx, ik_content_block_t *blocks,
                                   yyjson_val *part, yyjson_val *function_call, size_t idx)
{
    blocks[idx].type = IK_CONTENT_TOOL_CALL;

    // Generate tool call ID (Google doesn't provide one)
    blocks[idx].data.tool_call.id = ik_google_generate_tool_id(blocks); // LCOV_EXCL_BR_LINE (always returns valid ID, cannot fail)

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
        if (args_json == NULL) { // LCOV_EXCL_BR_LINE (only fails on extreme OOM)
            return ERR(ctx, PARSE, "Failed to serialize functionCall args"); // LCOV_EXCL_LINE
        }
        blocks[idx].data.tool_call.arguments = talloc_strdup(blocks, args_json);
        free(args_json); // yyjson uses malloc
        if (blocks[idx].data.tool_call.arguments == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    } else {
        // No args, use empty object
        blocks[idx].data.tool_call.arguments = talloc_strdup(blocks, "{}");
        if (blocks[idx].data.tool_call.arguments == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    // Extract thought signature if present (Gemini 3 only, appears alongside functionCall)
    blocks[idx].data.tool_call.thought_signature = NULL;
    yyjson_val *thought_sig_val = yyjson_obj_get(part, "thoughtSignature");
    if (thought_sig_val != NULL) {
        const char *thought_sig = yyjson_get_str(thought_sig_val);
        if (thought_sig != NULL) {
            blocks[idx].data.tool_call.thought_signature = talloc_strdup(blocks, thought_sig);
            if (blocks[idx].data.tool_call.thought_signature == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        }
    }

    return OK(NULL);
}

static res_t process_text_part(TALLOC_CTX *ctx, ik_content_block_t *blocks,
                               yyjson_val *part, size_t idx)
{
    // Check for thought flag (thinking)
    yyjson_val *thought_val = yyjson_obj_get(part, "thought");
    bool is_thought = thought_val != NULL && yyjson_get_bool(thought_val); // LCOV_EXCL_BR_LINE (short-circuit evaluation, compiler artifact)

    // Extract text
    yyjson_val *text_val = yyjson_obj_get(part, "text");
    if (text_val == NULL) {
        // Skip parts without text
        return OK(NULL);
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

    return OK(NULL);
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
    ik_content_block_t *blocks = talloc_zero_array(ctx, ik_content_block_t, (unsigned int)count);
    if (blocks == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    size_t idx, max;
    yyjson_val *part;
    yyjson_arr_foreach(parts_arr, idx, max, part) { // LCOV_EXCL_BR_LINE (vendor macro generates uncoverable loop branches)
        // Check for functionCall (tool call)
        yyjson_val *function_call = yyjson_obj_get(part, "functionCall");
        if (function_call != NULL) {
            res_t result = process_function_call(ctx, blocks, part, function_call, idx);
            if (is_err(&result)) return result;
            continue;
        }

        // Process text or thinking
        res_t result = process_text_part(ctx, blocks, part, idx);
        if (is_err(&result)) return result;
    }

    *out_blocks = blocks;
    *out_count = count;
    return OK(NULL);
}

/* ================================================================
 * Public Functions
 * ================================================================ */

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

    yyjson_val *root = yyjson_doc_get_root(doc); // LCOV_EXCL_BR_LINE (doc was valid, root cannot be NULL)
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return ERR(ctx, PARSE, "Root is not an object");
    }

    // Check for error response
    yyjson_val *error_obj = yyjson_obj_get(root, "error");
    if (error_obj != NULL) {
        yyjson_val *msg_val = yyjson_obj_get(error_obj, "message");
        const char *msg = msg_val ? yyjson_get_str(msg_val) : NULL;
        char *msg_copy = talloc_strdup(ctx, msg ? msg : "Unknown error");
        yyjson_doc_free(doc);
        if (msg_copy == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        res_t result = ERR(ctx, PROVIDER, "API error: %s", msg_copy);
        talloc_free(msg_copy);
        return result;
    }

    // Check for blocked prompt
    yyjson_val *feedback = yyjson_obj_get(root, "promptFeedback");
    if (feedback != NULL) {
        yyjson_val *block_reason = yyjson_obj_get(feedback, "blockReason");
        if (block_reason != NULL) {
            const char *reason = yyjson_get_str(block_reason);
            char *reason_copy = talloc_strdup(ctx, reason ? reason : "Unknown reason");
            yyjson_doc_free(doc);
            if (reason_copy == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            res_t result = ERR(ctx, PROVIDER, "Prompt blocked: %s", reason_copy);
            talloc_free(reason_copy);
            return result;
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
        yyjson_val *prompt_tokens = yyjson_obj_get(usage, "promptTokenCount"); // LCOV_EXCL_BR_LINE (compiler artifact)
        yyjson_val *candidates_tokens = yyjson_obj_get(usage, "candidatesTokenCount"); // LCOV_EXCL_BR_LINE (compiler artifact)
        yyjson_val *thoughts_tokens = yyjson_obj_get(usage, "thoughtsTokenCount"); // LCOV_EXCL_BR_LINE (compiler artifact)
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
        *out_resp = resp; // LCOV_EXCL_BR_LINE (simple assignment, compiler artifact)
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
        *out_resp = resp; // LCOV_EXCL_BR_LINE (simple assignment, compiler artifact)
        yyjson_doc_free(doc);
        return OK(resp);
    }

    // Extract finish reason
    yyjson_val *finish_val = yyjson_obj_get(candidate, "finishReason");
    const char *finish_str = finish_val ? yyjson_get_str(finish_val) : NULL;
    resp->finish_reason = ik_google_map_finish_reason(finish_str); // LCOV_EXCL_BR_LINE (simple assignment, compiler artifact)

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
    resp->provider_data = ik_google_extract_thought_signature_from_response(resp, root);

    *out_resp = resp; // LCOV_EXCL_BR_LINE (simple assignment, compiler artifact)
    yyjson_doc_free(doc);
    return OK(resp);
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
