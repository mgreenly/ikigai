// REPL HTTP callback handlers implementation
#include "repl_callbacks.h"
#include "repl.h"
#include "agent.h"
#include "repl_actions.h"
#include "shared.h"
#include "panic.h"
#include <assert.h>
#include <talloc.h>
#include <string.h>

/**
 * @brief Streaming callback for OpenAI API responses
 *
 * Called for each content chunk received during streaming.
 * Appends the chunk to the scrollback buffer.
 *
 * @param chunk   Content chunk (null-terminated string)
 * @param ctx     REPL context pointer
 * @return        OK(NULL) to continue, ERR(...) to abort
 */
res_t ik_repl_streaming_callback(const char *chunk, void *ctx)
{
    assert(chunk != NULL);  /* LCOV_EXCL_BR_LINE */
    assert(ctx != NULL);    /* LCOV_EXCL_BR_LINE */

    ik_repl_ctx_t *repl = (ik_repl_ctx_t *)ctx;
    size_t chunk_len = strlen(chunk);

    // Accumulate complete response for adding to conversation later
    if (repl->current->assistant_response == NULL) {
        repl->current->assistant_response = talloc_strdup(repl, chunk);
    } else {
        repl->current->assistant_response = talloc_strdup_append(repl->current->assistant_response, chunk);
    }
    if (repl->current->assistant_response == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Handle streaming display with line buffering
    // Accumulate chunks until we hit a newline, then flush to scrollback
    size_t start = 0;
    for (size_t i = 0; i < chunk_len; i++) {
        if (chunk[i] == '\n') {
            // Flush buffered line (if any) plus characters up to newline
            size_t prefix_len = i - start;  // Characters before newline in this segment
            if (repl->current->streaming_line_buffer != NULL) {
                // Append prefix to buffer
                size_t buffer_len = strlen(repl->current->streaming_line_buffer);
                size_t total_len = buffer_len + prefix_len;
                char *line = talloc_size(repl, total_len + 1);
                if (line == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
                memcpy(line, repl->current->streaming_line_buffer, buffer_len);
                memcpy(line + buffer_len, chunk + start, prefix_len);
                line[total_len] = '\0';

                ik_scrollback_append_line(repl->current->scrollback, line, total_len);
                talloc_free(line);
                talloc_free(repl->current->streaming_line_buffer);
                repl->current->streaming_line_buffer = NULL;
            } else if (prefix_len > 0) {
                // No buffer, just flush the prefix
                ik_scrollback_append_line(repl->current->scrollback, chunk + start, prefix_len);
            } else {
                // Empty line (just a newline)
                ik_scrollback_append_line(repl->current->scrollback, "", 0);
            }

            // Start next segment after newline
            start = i + 1;
        }
    }

    // Buffer any remaining characters (no newline found)
    if (start < chunk_len) {
        size_t remaining_len = chunk_len - start;
        if (repl->current->streaming_line_buffer == NULL) {
            repl->current->streaming_line_buffer = talloc_strndup(repl, chunk + start, remaining_len);
        } else {
            repl->current->streaming_line_buffer = talloc_strndup_append_buffer(repl->current->streaming_line_buffer,
                                                                       chunk + start,
                                                                       remaining_len);
        }
        if (repl->current->streaming_line_buffer == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    // Trigger re-render to show streaming content
    ik_repl_render_frame(repl);

    return OK(NULL);
}

/**
 * @brief Completion callback for HTTP requests
 *
 * Called when an HTTP request completes (success or failure).
 * Stores error information in REPL context for display by completion handler.
 *
 * @param completion   Completion information (status, error message)
 * @param ctx          REPL context pointer
 * @return             OK(NULL) on success, ERR(...) on failure
 */
res_t ik_repl_http_completion_callback(const ik_http_completion_t *completion, void *ctx)
{
    assert(completion != NULL);  /* LCOV_EXCL_BR_LINE */
    assert(ctx != NULL);         /* LCOV_EXCL_BR_LINE */

    ik_repl_ctx_t *repl = (ik_repl_ctx_t *)ctx;

    // Debug output for response metadata
    if (repl->shared->openai_debug_pipe != NULL && repl->shared->openai_debug_pipe->write_end != NULL) {
        fprintf(repl->shared->openai_debug_pipe->write_end,
                "<< RESPONSE: type=%s",
                completion->type == IK_HTTP_SUCCESS ? "success" : "error");
        if (completion->type == IK_HTTP_SUCCESS) {
            fprintf(repl->shared->openai_debug_pipe->write_end,
                    ", model=%s, finish=%s, tokens=%d",
                    completion->model ? completion->model : "(null)",
                    completion->finish_reason ? completion->finish_reason : "(null)",
                    completion->completion_tokens);
        }
        if (completion->tool_call != NULL) {
            fprintf(repl->shared->openai_debug_pipe->write_end,
                    ", tool_call=%s(%s)",
                    completion->tool_call->name,
                    completion->tool_call->arguments);
        }
        fprintf(repl->shared->openai_debug_pipe->write_end, "\n");
        fflush(repl->shared->openai_debug_pipe->write_end);
    }

    // Flush any remaining buffered line content (streaming ended without final newline)
    if (repl->current->streaming_line_buffer != NULL) {
        size_t buffer_len = strlen(repl->current->streaming_line_buffer);
        ik_scrollback_append_line(repl->current->scrollback, repl->current->streaming_line_buffer, buffer_len);
        talloc_free(repl->current->streaming_line_buffer);
        repl->current->streaming_line_buffer = NULL;
    }

    // Add blank line after assistant response (spacing)
    if (completion->type == IK_HTTP_SUCCESS) {
        ik_scrollback_append_line(repl->current->scrollback, "", 0);
    }

    // Clear any previous error
    if (repl->current->http_error_message != NULL) {
        talloc_free(repl->current->http_error_message);
        repl->current->http_error_message = NULL;
    }

    // Store error message if request failed
    if (completion->type != IK_HTTP_SUCCESS && completion->error_message != NULL) {
        repl->current->http_error_message = talloc_strdup(repl, completion->error_message);
        if (repl->current->http_error_message == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    // Store response metadata for database persistence (on success only)
    if (completion->type == IK_HTTP_SUCCESS) {
        // Clear previous metadata
        if (repl->current->response_model != NULL) {
            talloc_free(repl->current->response_model);
            repl->current->response_model = NULL;
        }
        if (repl->current->response_finish_reason != NULL) {
            talloc_free(repl->current->response_finish_reason);
            repl->current->response_finish_reason = NULL;
        }
        repl->current->response_completion_tokens = 0;

        // Store new metadata
        if (completion->model != NULL) {
            repl->current->response_model = talloc_strdup(repl, completion->model);
            if (repl->current->response_model == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        }
        if (completion->finish_reason != NULL) {
            repl->current->response_finish_reason = talloc_strdup(repl, completion->finish_reason);
            if (repl->current->response_finish_reason == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        }
        repl->current->response_completion_tokens = completion->completion_tokens;

        // Store tool_call if present
        if (repl->current->pending_tool_call != NULL) {
            talloc_free(repl->current->pending_tool_call);
            repl->current->pending_tool_call = NULL;
        }
        if (completion->tool_call != NULL) {
            // Deep copy the tool_call struct
            repl->current->pending_tool_call = ik_tool_call_create(repl,
                                                          completion->tool_call->id,
                                                          completion->tool_call->name,
                                                          completion->tool_call->arguments);
            if (repl->current->pending_tool_call == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        }
    }

    return OK(NULL);
}
