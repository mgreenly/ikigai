/**
 * @file openai_handlers.c
 * @brief OpenAI HTTP completion handlers
 */

#include "openai_handlers.h"
#include "error.h"
#include "panic.h"
#include "response.h"
#include "streaming.h"
#include <assert.h>
#include <string.h>

/* ================================================================
 * HTTP Completion Callback
 * ================================================================ */

/**
 * HTTP completion callback for non-streaming requests
 *
 * Called from info_read() when HTTP transfer completes.
 * Parses response and invokes user's completion callback.
 */
void ik_openai_http_completion_handler(const ik_http_completion_t *http_completion, void *user_ctx)
{
    ik_openai_request_ctx_t *req_ctx = (ik_openai_request_ctx_t *)user_ctx;
    assert(req_ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(req_ctx->cb != NULL);  // LCOV_EXCL_BR_LINE

    // Build provider completion structure
    ik_provider_completion_t provider_completion = {0};
    provider_completion.http_status = http_completion->http_code;

    // Handle HTTP errors
    if (http_completion->type != IK_HTTP_SUCCESS) {
        provider_completion.success = false;
        provider_completion.response = NULL;
        provider_completion.retry_after_ms = -1;

        // Parse error response if we have JSON body
        if (http_completion->response_body != NULL && http_completion->response_len > 0) {
            ik_error_category_t category;
            char *error_msg = NULL;
            res_t parse_res = ik_openai_parse_error(req_ctx, http_completion->http_code,
                                                     http_completion->response_body,
                                                     http_completion->response_len,
                                                     &category, &error_msg);
            if (is_ok(&parse_res)) {
                provider_completion.error_category = category;
                provider_completion.error_message = error_msg;
            } else {
                // Fallback to generic error message
                provider_completion.error_category = IK_ERR_CAT_UNKNOWN;
                provider_completion.error_message = talloc_asprintf(req_ctx,
                    "HTTP %d error", http_completion->http_code);
            }
        } else {
            // Network error or no response body
            if (http_completion->http_code == 0) {
                provider_completion.error_category = IK_ERR_CAT_NETWORK;
            } else {
                provider_completion.error_category = IK_ERR_CAT_UNKNOWN;
            }
            provider_completion.error_message = http_completion->error_message != NULL
                ? talloc_strdup(req_ctx, http_completion->error_message)
                : talloc_asprintf(req_ctx, "HTTP %d error", http_completion->http_code);
        }

        // Invoke user callback with error
        req_ctx->cb(&provider_completion, req_ctx->cb_ctx);

        // Cleanup
        talloc_free(req_ctx);
        return;
    }

    // Parse successful response
    ik_response_t *response = NULL;
    res_t parse_res;

    if (req_ctx->use_responses_api) {
        parse_res = ik_openai_parse_responses_response(req_ctx,
                                                        http_completion->response_body,
                                                        http_completion->response_len,
                                                        &response);
    } else {
        parse_res = ik_openai_parse_chat_response(req_ctx,
                                                   http_completion->response_body,
                                                   http_completion->response_len,
                                                   &response);
    }

    if (is_err(&parse_res)) {
        // Parse error
        provider_completion.success = false;
        provider_completion.response = NULL;
        provider_completion.error_category = IK_ERR_CAT_UNKNOWN;
        provider_completion.error_message = talloc_asprintf(req_ctx,
            "Failed to parse response: %s", parse_res.err->msg);
        provider_completion.retry_after_ms = -1;

        // Invoke user callback with error
        req_ctx->cb(&provider_completion, req_ctx->cb_ctx);

        // Cleanup
        talloc_free(req_ctx);
        return;
    }

    // Success - invoke callback with parsed response
    provider_completion.success = true;
    provider_completion.response = response;
    provider_completion.error_category = IK_ERR_CAT_UNKNOWN;  // Not used on success
    provider_completion.error_message = NULL;
    provider_completion.retry_after_ms = -1;

    req_ctx->cb(&provider_completion, req_ctx->cb_ctx);

    // Cleanup request context
    talloc_free(req_ctx);
}

/* ================================================================
 * Streaming Callbacks
 * ================================================================ */

/**
 * curl write callback for SSE streaming
 *
 * Called during perform() as HTTP chunks arrive.
 * Parses SSE format and feeds data events to parser.
 */
size_t ik_openai_stream_write_callback(const char *data, size_t len, void *userdata)
{
    ik_openai_stream_request_ctx_t *req_ctx = (ik_openai_stream_request_ctx_t *)userdata;
    assert(req_ctx != NULL);  // LCOV_EXCL_BR_LINE

    // Append to buffer (handles incomplete lines across chunks)
    char *new_buffer = talloc_realloc(req_ctx, req_ctx->sse_buffer,
                                       char, (unsigned int)(req_ctx->sse_buffer_len + len + 1));
    if (new_buffer == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    memcpy(new_buffer + req_ctx->sse_buffer_len, data, len);
    req_ctx->sse_buffer_len += len;
    new_buffer[req_ctx->sse_buffer_len] = '\0';
    req_ctx->sse_buffer = new_buffer;

    // Process complete lines
    char *line_start = req_ctx->sse_buffer;
    char *line_end;

    while ((line_end = strchr(line_start, '\n')) != NULL) {
        *line_end = '\0';

        // Check for "data: " prefix
        if (strncmp(line_start, "data: ", 6) == 0) {
            const char *json_data = line_start + 6;

            // Feed to parser - this invokes user's stream_cb
            ik_openai_chat_stream_process_data(req_ctx->parser_ctx, json_data);
        }

        // Move to next line
        line_start = line_end + 1;
    }

    // Keep incomplete line in buffer
    size_t remaining = req_ctx->sse_buffer_len - (size_t)(line_start - req_ctx->sse_buffer);
    if (remaining > 0) {
        memmove(req_ctx->sse_buffer, line_start, remaining);
        req_ctx->sse_buffer_len = remaining;
        req_ctx->sse_buffer[remaining] = '\0';
    } else {
        talloc_free(req_ctx->sse_buffer);
        req_ctx->sse_buffer = NULL;
        req_ctx->sse_buffer_len = 0;
    }

    return len;
}

/**
 * HTTP completion callback for streaming requests
 *
 * Called from info_read() when HTTP transfer completes.
 * Invokes user's completion callback with final metadata.
 */
void ik_openai_stream_completion_handler(const ik_http_completion_t *http_completion,
                                          void *user_ctx)
{
    ik_openai_stream_request_ctx_t *req_ctx = (ik_openai_stream_request_ctx_t *)user_ctx;
    assert(req_ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(req_ctx->completion_cb != NULL);  // LCOV_EXCL_BR_LINE

    // Build provider completion structure
    ik_provider_completion_t provider_completion = {0};
    provider_completion.http_status = http_completion->http_code;

    // Handle HTTP errors
    if (http_completion->type != IK_HTTP_SUCCESS) {
        provider_completion.success = false;
        provider_completion.response = NULL;
        provider_completion.retry_after_ms = -1;

        // Parse error if JSON body available
        if (http_completion->response_body != NULL && http_completion->response_len > 0) {
            ik_error_category_t category;
            char *error_msg = NULL;
            res_t parse_res = ik_openai_parse_error(req_ctx, http_completion->http_code,
                                                     http_completion->response_body,
                                                     http_completion->response_len,
                                                     &category, &error_msg);
            if (is_ok(&parse_res)) {
                provider_completion.error_category = category;
                provider_completion.error_message = error_msg;
            } else {
                provider_completion.error_category = IK_ERR_CAT_UNKNOWN;
                provider_completion.error_message = talloc_asprintf(req_ctx,
                    "HTTP %d error", http_completion->http_code);
            }
        } else {
            if (http_completion->http_code == 0) {
                provider_completion.error_category = IK_ERR_CAT_NETWORK;
            } else {
                provider_completion.error_category = IK_ERR_CAT_UNKNOWN;
            }
            provider_completion.error_message = http_completion->error_message != NULL
                ? talloc_strdup(req_ctx, http_completion->error_message)
                : talloc_asprintf(req_ctx, "HTTP %d error", http_completion->http_code);
        }

        req_ctx->completion_cb(&provider_completion, req_ctx->completion_ctx);
        talloc_free(req_ctx);
        return;
    }

    // Success - stream events were already delivered during perform()
    provider_completion.success = true;
    provider_completion.response = NULL;
    provider_completion.error_category = IK_ERR_CAT_UNKNOWN;
    provider_completion.error_message = NULL;
    provider_completion.retry_after_ms = -1;

    req_ctx->completion_cb(&provider_completion, req_ctx->completion_ctx);

    // Cleanup
    talloc_free(req_ctx);
}
