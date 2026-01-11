// REPL HTTP callback handlers implementation
#include "repl_callbacks.h"
#include "repl.h"
#include "agent.h"
#include "ansi.h"
#include "event_render.h"
#include "repl_actions.h"
#include "shared.h"
#include "panic.h"
#include "logger.h"
#include "vendor/yyjson/yyjson.h"
#include <assert.h>
#include <talloc.h>
#include <string.h>

/**
 * Helper to flush a complete line to scrollback
 */
static void flush_line_to_scrollback(ik_agent_ctx_t *agent, const char *chunk,
                                     size_t start, size_t prefix_len)
{
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
}

/**
 * Helper to handle text delta streaming with line buffering
 */
static void handle_text_delta(ik_agent_ctx_t *agent, const char *chunk, size_t chunk_len)
{
    // Accumulate complete response for adding to conversation later
    if (agent->assistant_response == NULL) {
        agent->assistant_response = talloc_strdup(agent, chunk);
    } else {
        agent->assistant_response = talloc_strdup_append(agent->assistant_response, chunk);
    }
    if (agent->assistant_response == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Handle streaming display with line buffering
    size_t start = 0;
    for (size_t i = 0; i < chunk_len; i++) {
        if (chunk[i] == '\n') {
            flush_line_to_scrollback(agent, chunk, start, i - start);
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

    switch (event->type) { // LCOV_EXCL_BR_LINE - default case is defensive
        case IK_STREAM_START:
            if (agent->assistant_response != NULL) {
                talloc_free(agent->assistant_response);
                agent->assistant_response = NULL;
            }
            break;

        case IK_STREAM_TEXT_DELTA:
            if (event->data.delta.text != NULL) {
                handle_text_delta(agent, event->data.delta.text, strlen(event->data.delta.text));
            }
            break;

        case IK_STREAM_THINKING_DELTA:
            // Accumulate thinking content (not displayed in scrollback during streaming)
            break;

        case IK_STREAM_TOOL_CALL_START:
        case IK_STREAM_TOOL_CALL_DELTA:
        case IK_STREAM_TOOL_CALL_DONE:
            // No-op: provider accumulates tool calls and builds response
            break;

        case IK_STREAM_DONE:
            agent->response_input_tokens = event->data.done.usage.input_tokens;
            agent->response_output_tokens = event->data.done.usage.output_tokens;
            agent->response_thinking_tokens = event->data.done.usage.thinking_tokens;
            break;

        case IK_STREAM_ERROR:
            if (event->data.error.message != NULL) {
                if (agent->http_error_message != NULL) {
                    talloc_free(agent->http_error_message);
                }
                agent->http_error_message = talloc_strdup(agent, event->data.error.message);
                if (agent->http_error_message == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            }
            break;

        default: // LCOV_EXCL_LINE - defensive: all event types handled above
            break; // LCOV_EXCL_LINE
    }

    return OK(NULL);
}

/**
 * Helper to render usage event with token counts
 */
static void render_usage_event(ik_agent_ctx_t *agent)
{
    int32_t total = agent->response_input_tokens + agent->response_output_tokens +
                    agent->response_thinking_tokens;
    if (total > 0) {
        char data_json[256];
        snprintf(data_json, sizeof(data_json),
                 "{\"input_tokens\":%d,\"output_tokens\":%d,\"thinking_tokens\":%d}",
                 agent->response_input_tokens, agent->response_output_tokens,
                 agent->response_thinking_tokens);
        ik_event_render(agent->scrollback, "usage", NULL, data_json);
    } else {
        ik_scrollback_append_line(agent->scrollback, "", 0);
    }
}

/**
 * Helper to store response metadata
 */
static void store_response_metadata(ik_agent_ctx_t *agent, const ik_response_t *response)
{
    // Clear previous metadata
    if (agent->response_model != NULL) {
        talloc_free(agent->response_model);
        agent->response_model = NULL;
    }
    if (agent->response_finish_reason != NULL) {
        talloc_free(agent->response_finish_reason);
        agent->response_finish_reason = NULL;
    }

    // Store model
    if (response->model != NULL) {
        agent->response_model = talloc_strdup(agent, response->model);
        if (agent->response_model == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    // Map finish reason to string
    const char *finish_reason_str = "unknown";
    switch (response->finish_reason) { // LCOV_EXCL_BR_LINE - all enum values handled
        case IK_FINISH_STOP: finish_reason_str = "stop"; break;
        case IK_FINISH_LENGTH: finish_reason_str = "length"; break;
        case IK_FINISH_TOOL_USE: finish_reason_str = "tool_use"; break;
        case IK_FINISH_CONTENT_FILTER: finish_reason_str = "content_filter"; break;
        case IK_FINISH_ERROR: finish_reason_str = "error"; break;
        case IK_FINISH_UNKNOWN: finish_reason_str = "unknown"; break;
    }
    agent->response_finish_reason = talloc_strdup(agent, finish_reason_str);
    if (agent->response_finish_reason == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    // Store token counts
    agent->response_input_tokens = response->usage.input_tokens;
    agent->response_output_tokens = response->usage.output_tokens;
    agent->response_thinking_tokens = response->usage.thinking_tokens;
}

/**
 * Helper to extract tool calls and thinking blocks from response
 */
static void extract_tool_calls(ik_agent_ctx_t *agent, const ik_response_t *response)
{
    // Clear any previous pending thinking
    if (agent->pending_thinking_text != NULL) {
        talloc_free(agent->pending_thinking_text);
        agent->pending_thinking_text = NULL;
    }
    if (agent->pending_thinking_signature != NULL) {
        talloc_free(agent->pending_thinking_signature);
        agent->pending_thinking_signature = NULL;
    }
    if (agent->pending_redacted_data != NULL) {
        talloc_free(agent->pending_redacted_data);
        agent->pending_redacted_data = NULL;
    }

    // Clear any previous pending tool call
    if (agent->pending_tool_call != NULL) {
        talloc_free(agent->pending_tool_call);
        agent->pending_tool_call = NULL;
    }

    for (size_t i = 0; i < response->content_count; i++) {
        ik_content_block_t *block = &response->content_blocks[i];

        if (block->type == IK_CONTENT_THINKING) {
            if (block->data.thinking.text != NULL) {
                agent->pending_thinking_text = talloc_strdup(agent, block->data.thinking.text);
                if (agent->pending_thinking_text == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            }
            if (block->data.thinking.signature != NULL) {
                agent->pending_thinking_signature = talloc_strdup(agent, block->data.thinking.signature);
                if (agent->pending_thinking_signature == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            }
        } else if (block->type == IK_CONTENT_REDACTED_THINKING) {
            if (block->data.redacted_thinking.data != NULL) {
                agent->pending_redacted_data = talloc_strdup(agent, block->data.redacted_thinking.data);
                if (agent->pending_redacted_data == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            }
        } else if (block->type == IK_CONTENT_TOOL_CALL) {
            // TODO: Replace stub with proper tool call creation when tool.c is rebuilt
            agent->pending_tool_call = NULL;
            break;  // Only handle first tool call
        }
    }
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

        ik_logger_debug_json(agent->shared->logger, doc);  // LCOV_EXCL_LINE
    }

    // Flush any remaining buffered line content
    if (agent->streaming_line_buffer != NULL) {
        size_t buffer_len = strlen(agent->streaming_line_buffer);
        ik_scrollback_append_line(agent->scrollback, agent->streaming_line_buffer, buffer_len);
        talloc_free(agent->streaming_line_buffer);
        agent->streaming_line_buffer = NULL;
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
        store_response_metadata(agent, completion->response);
        render_usage_event(agent);
        extract_tool_calls(agent, completion->response);
    }

    return OK(NULL);
}
