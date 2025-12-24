// REPL HTTP callback handlers implementation
#include "repl_callbacks.h"
#include "repl.h"
#include "agent.h"
#include "repl_actions.h"
#include "shared.h"
#include "panic.h"
#include "logger.h"
#include "vendor/yyjson/yyjson.h"
#include <assert.h>
#include <talloc.h>
#include <string.h>

/**
 * @brief Stream callback for provider API responses
 *
 * Called during perform() as data arrives from the network.
 * Handles normalized stream events (text deltas, thinking, tool calls, etc.).
 * Updates UI incrementally as content streams in.
 *
 * @param event   Stream event (text delta, tool call, etc.)
 * @param ctx     Agent context pointer
 * @return        OK(NULL) to continue, ERR(...) to abort
 */
res_t ik_repl_stream_callback(const ik_stream_event_t *event, void *ctx)
{
    assert(event != NULL);  /* LCOV_EXCL_BR_LINE */
    assert(ctx != NULL);    /* LCOV_EXCL_BR_LINE */

    ik_agent_ctx_t *agent = (ik_agent_ctx_t *)ctx;

    switch (event->type) {
        case IK_STREAM_START:
            // Initialize streaming response (clear previous state if any)
            if (agent->assistant_response != NULL) {
                talloc_free(agent->assistant_response);
                agent->assistant_response = NULL;
            }
            break;

        case IK_STREAM_TEXT_DELTA:
            if (event->data.delta.text != NULL) {
                const char *chunk = event->data.delta.text;
                size_t chunk_len = strlen(chunk);

                // Accumulate complete response for adding to conversation later
                if (agent->assistant_response == NULL) {
                    agent->assistant_response = talloc_strdup(agent, chunk);
                } else {
                    agent->assistant_response = talloc_strdup_append(agent->assistant_response, chunk);
                }
                if (agent->assistant_response == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

                // Handle streaming display with line buffering
                // Accumulate chunks until we hit a newline, then flush to scrollback
                size_t start = 0;
                for (size_t i = 0; i < chunk_len; i++) {
                    if (chunk[i] == '\n') {
                        // Flush buffered line (if any) plus characters up to newline
                        size_t prefix_len = i - start;  // Characters before newline in this segment
                        if (agent->streaming_line_buffer != NULL) {
                            // Append prefix to buffer
                            size_t buffer_len = strlen(agent->streaming_line_buffer);
                            size_t total_len = buffer_len + prefix_len;
                            char *line = talloc_size(agent, total_len + 1);
                            if (line == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
                            memcpy(line, agent->streaming_line_buffer, buffer_len);
                            memcpy(line + buffer_len, chunk + start, prefix_len);
                            line[total_len] = '\0';

                            ik_scrollback_append_line(agent->scrollback, line, total_len);
                            talloc_free(line);
                            talloc_free(agent->streaming_line_buffer);
                            agent->streaming_line_buffer = NULL;
                        } else if (prefix_len > 0) {
                            // No buffer, just flush the prefix
                            ik_scrollback_append_line(agent->scrollback, chunk + start, prefix_len);
                        } else {
                            // Empty line (just a newline)
                            ik_scrollback_append_line(agent->scrollback, "", 0);
                        }

                        // Start next segment after newline
                        start = i + 1;
                    }
                }

                // Buffer any remaining characters (no newline found)
                if (start < chunk_len) {
                    size_t remaining_len = chunk_len - start;
                    if (agent->streaming_line_buffer == NULL) {
                        agent->streaming_line_buffer = talloc_strndup(agent, chunk + start, remaining_len);
                    } else {
                        agent->streaming_line_buffer = talloc_strndup_append_buffer(agent->streaming_line_buffer,
                                                                                    chunk + start,
                                                                                    remaining_len);
                    }
                    if (agent->streaming_line_buffer == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
                }
            }
            break;

        case IK_STREAM_THINKING_DELTA:
            // Accumulate thinking content (not displayed in scrollback during streaming)
            // Will be stored in database on completion
            // For now, we skip thinking display during streaming
            break;

        case IK_STREAM_TOOL_CALL_START:
            // Tool call started - for now we don't handle streaming tool calls
            // The old code accumulated tool calls in completion callback
            // We'll maintain that behavior for now
            break;

        case IK_STREAM_TOOL_CALL_DELTA:
            // Tool call argument accumulation - not implemented yet
            break;

        case IK_STREAM_TOOL_CALL_DONE:
            // Tool call finalized - not implemented yet
            break;

        case IK_STREAM_DONE:
            // Stream complete - handled in completion callback
            break;

        case IK_STREAM_ERROR:
            // Error during streaming
            if (event->data.error.message != NULL) {
                // Store error for display in completion handler
                if (agent->http_error_message != NULL) {
                    talloc_free(agent->http_error_message);
                }
                agent->http_error_message = talloc_strdup(agent, event->data.error.message);
                if (agent->http_error_message == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            }
            break;

        default:
            // Unknown event type - ignore
            break;
    }

    // NOTE: No render call - event loop handles rendering
    return OK(NULL);
}

/**
 * @brief Completion callback for provider requests
 *
 * Called from info_read() when an HTTP request completes (success or failure).
 * Stores response metadata and finalizes streaming state.
 *
 * @param completion   Completion information (usage, metadata, error)
 * @param ctx          Agent context pointer
 * @return             OK(NULL) on success, ERR(...) on failure
 */
res_t ik_repl_completion_callback(const ik_provider_completion_t *completion, void *ctx)
{
    assert(completion != NULL);  /* LCOV_EXCL_BR_LINE */
    assert(ctx != NULL);         /* LCOV_EXCL_BR_LINE */

    ik_agent_ctx_t *agent = (ik_agent_ctx_t *)ctx;

    // Log response metadata via JSONL logger
    {
        yyjson_mut_doc *doc = ik_log_create();  // LCOV_EXCL_LINE
        yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(doc, root, "event", "provider_response");  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(doc, root, "type",  // LCOV_EXCL_LINE
                               completion->success ? "success" : "error");  // LCOV_EXCL_LINE

        if (completion->success && completion->response != NULL) {  // LCOV_EXCL_BR_LINE
            yyjson_mut_obj_add_str(doc, root, "model",  // LCOV_EXCL_LINE
                                   completion->response->model ? completion->response->model : "(null)");  // LCOV_EXCL_LINE
            yyjson_mut_obj_add_int(doc, root, "input_tokens", completion->response->usage.input_tokens);  // LCOV_EXCL_LINE
            yyjson_mut_obj_add_int(doc, root, "output_tokens", completion->response->usage.output_tokens);  // LCOV_EXCL_LINE
            yyjson_mut_obj_add_int(doc, root, "thinking_tokens", completion->response->usage.thinking_tokens);  // LCOV_EXCL_LINE
            yyjson_mut_obj_add_int(doc, root, "total_tokens", completion->response->usage.total_tokens);  // LCOV_EXCL_LINE
        }  // LCOV_EXCL_LINE

        // DI pattern: use explicit logger from shared context
        ik_logger_debug_json(agent->shared->logger, doc);  // LCOV_EXCL_LINE
    }

    // Flush any remaining buffered line content (streaming ended without final newline)
    if (agent->streaming_line_buffer != NULL) {
        size_t buffer_len = strlen(agent->streaming_line_buffer);
        ik_scrollback_append_line(agent->scrollback, agent->streaming_line_buffer, buffer_len);
        talloc_free(agent->streaming_line_buffer);
        agent->streaming_line_buffer = NULL;
    }

    // Add blank line after assistant response (spacing)
    if (completion->success) {
        ik_scrollback_append_line(agent->scrollback, "", 0);
    }

    // Clear any previous error
    if (agent->http_error_message != NULL) {
        talloc_free(agent->http_error_message);
        agent->http_error_message = NULL;
    }

    // Store error message if request failed
    if (!completion->success && completion->error_message != NULL) {
        agent->http_error_message = talloc_strdup(agent, completion->error_message);
        if (agent->http_error_message == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    // Store response metadata for database persistence (on success only)
    if (completion->success && completion->response != NULL) {
        // Clear previous metadata
        if (agent->response_model != NULL) {
            talloc_free(agent->response_model);
            agent->response_model = NULL;
        }
        if (agent->response_finish_reason != NULL) {
            talloc_free(agent->response_finish_reason);
            agent->response_finish_reason = NULL;
        }
        agent->response_completion_tokens = 0;

        // Store new metadata
        if (completion->response->model != NULL) {
            agent->response_model = talloc_strdup(agent, completion->response->model);
            if (agent->response_model == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        }

        // Map finish reason to string
        const char *finish_reason_str = "unknown";
        switch (completion->response->finish_reason) {
            case IK_FINISH_STOP: finish_reason_str = "stop"; break;
            case IK_FINISH_LENGTH: finish_reason_str = "length"; break;
            case IK_FINISH_TOOL_USE: finish_reason_str = "tool_use"; break;
            case IK_FINISH_CONTENT_FILTER: finish_reason_str = "content_filter"; break;
            case IK_FINISH_ERROR: finish_reason_str = "error"; break;
            case IK_FINISH_UNKNOWN: finish_reason_str = "unknown"; break;
        }
        agent->response_finish_reason = talloc_strdup(agent, finish_reason_str);
        if (agent->response_finish_reason == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        // Store token counts (output_tokens for backward compatibility)
        agent->response_completion_tokens = completion->response->usage.output_tokens;

        // Handle tool calls from response content blocks
        if (agent->pending_tool_call != NULL) {
            talloc_free(agent->pending_tool_call);
            agent->pending_tool_call = NULL;
        }

        // Look for tool call in content blocks
        for (size_t i = 0; i < completion->response->content_count; i++) {
            ik_content_block_t *block = &completion->response->content_blocks[i];
            if (block->type == IK_CONTENT_TOOL_CALL) {
                // Found a tool call - store it for execution
                agent->pending_tool_call = ik_tool_call_create(agent,
                                                               block->data.tool_call.id,
                                                               block->data.tool_call.name,
                                                               block->data.tool_call.arguments);
                if (agent->pending_tool_call == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
                // Only handle first tool call for now
                break;
            }
        }
    }

    return OK(NULL);
}
