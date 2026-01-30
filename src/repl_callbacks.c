// REPL HTTP callback handlers implementation
#include "repl_callbacks.h"
#include "repl.h"
#include "agent.h"
#include "ansi.h"
#include "debug_log.h"
#include "event_render.h"
#include "output_style.h"
#include "repl_actions.h"
#include "shared.h"
#include "panic.h"
#include "logger.h"
#include "vendor/yyjson/yyjson.h"
#include <assert.h>
#include <talloc.h>
#include <string.h>

static void flush_line_to_scrollback(ik_agent_ctx_t *agent, const char *chunk,
                                     size_t start, size_t chunk_len)
{
    const char *model_prefix = ik_output_prefix(IK_OUTPUT_MODEL_TEXT);
    size_t prefix_bytes = agent->streaming_first_line && model_prefix ? strlen(model_prefix) + 1 : 0;

    if (agent->streaming_line_buffer != NULL) {
        // Append chunk segment to buffer
        size_t buffer_len = strlen(agent->streaming_line_buffer);
        size_t total_len = prefix_bytes + buffer_len + chunk_len;
        char *line = talloc_size(agent, total_len + 1);
        if (line == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

        size_t pos = 0;
        if (prefix_bytes > 0) {
            memcpy(line, model_prefix, prefix_bytes - 1);
            line[prefix_bytes - 1] = ' ';
            pos = prefix_bytes;
        }
        memcpy(line + pos, agent->streaming_line_buffer, buffer_len);
        memcpy(line + pos + buffer_len, chunk + start, chunk_len);
        line[total_len] = '\0';

        ik_scrollback_append_line(agent->scrollback, line, total_len);
        talloc_free(line);
        talloc_free(agent->streaming_line_buffer);
        agent->streaming_line_buffer = NULL;
    } else if (chunk_len > 0) {
        // No buffer, just flush the chunk segment
        if (prefix_bytes > 0) {
            size_t total_len = prefix_bytes + chunk_len;
            char *line = talloc_size(agent, total_len + 1);
            if (line == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            memcpy(line, model_prefix, prefix_bytes - 1);
            line[prefix_bytes - 1] = ' ';
            memcpy(line + prefix_bytes, chunk + start, chunk_len);
            line[total_len] = '\0';
            ik_scrollback_append_line(agent->scrollback, line, total_len);
            talloc_free(line);
        } else {
            ik_scrollback_append_line(agent->scrollback, chunk + start, chunk_len);
        }
    } else {
        // Empty line (just a newline)
        if (prefix_bytes > 0) {
            size_t total_len = prefix_bytes;
            char *line = talloc_size(agent, total_len + 1);
            if (line == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            memcpy(line, model_prefix, prefix_bytes - 1);
            line[prefix_bytes - 1] = ' ';
            line[total_len] = '\0';
            ik_scrollback_append_line(agent->scrollback, line, total_len);
            talloc_free(line);
        } else {
            ik_scrollback_append_line(agent->scrollback, "", 0);
        }
    }

    agent->streaming_first_line = false;
}

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
            agent->streaming_first_line = true;
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

static void store_response_metadata(ik_agent_ctx_t *agent, const ik_response_t *response)
{
    DEBUG_LOG("store_response_metadata: ENTRY agent=%p response=%p", (void *)agent, (const void *)response);
    DEBUG_LOG("store_response_metadata: response->model=%s finish_reason=%d",
              response->model ? response->model : "(null)", response->finish_reason);

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

static void extract_tool_calls(ik_agent_ctx_t *agent, const ik_response_t *response)
{
    DEBUG_LOG("extract_tool_calls: ENTRY agent=%p response=%p", (void *)agent, (const void *)response);
    DEBUG_LOG("extract_tool_calls: response->content_count=%zu response->content_blocks=%p",
              response->content_count, (void *)response->content_blocks);

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
    if (agent->pending_tool_thought_signature != NULL) {
        talloc_free(agent->pending_tool_thought_signature);
        agent->pending_tool_thought_signature = NULL;
    }

    DEBUG_LOG("extract_tool_calls: iterating over %zu content blocks", response->content_count);
    for (size_t i = 0; i < response->content_count; i++) {
        DEBUG_LOG("extract_tool_calls: accessing block[%zu] at %p", i, (void *)response->content_blocks);
        ik_content_block_t *block = &response->content_blocks[i];
        DEBUG_LOG("extract_tool_calls: block[%zu]=%p type=%d", i, (void *)block, block->type);

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
            DEBUG_LOG("extract_tool_calls: TOOL_CALL branch entered");
            DEBUG_LOG("extract_tool_calls: tool_call.id=%p tool_call.name=%p tool_call.arguments=%p",
                      (void *)block->data.tool_call.id,
                      (void *)block->data.tool_call.name,
                      (void *)block->data.tool_call.arguments);
            DEBUG_LOG("extract_tool_calls: id='%s' name='%s' args='%.100s'",
                      block->data.tool_call.id ? block->data.tool_call.id : "(null)",
                      block->data.tool_call.name ? block->data.tool_call.name : "(null)",
                      block->data.tool_call.arguments ? block->data.tool_call.arguments : "(null)");
            agent->pending_tool_call = ik_tool_call_create(agent,
                                                           block->data.tool_call.id,
                                                           block->data.tool_call.name,
                                                           block->data.tool_call.arguments);
            DEBUG_LOG("extract_tool_calls: ik_tool_call_create returned %p", (void *)agent->pending_tool_call);
            if (agent->pending_tool_call == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            DEBUG_LOG("extract_tool_calls: checking thought_signature ptr=%p", (void *)block->data.tool_call.thought_signature);
            if (block->data.tool_call.thought_signature != NULL) {
                DEBUG_LOG("extract_tool_calls: thought_signature is non-NULL, copying...");
                agent->pending_tool_thought_signature = talloc_strdup(agent, block->data.tool_call.thought_signature);
                DEBUG_LOG("extract_tool_calls: talloc_strdup returned %p", (void *)agent->pending_tool_thought_signature);
                if (agent->pending_tool_thought_signature == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            }
            DEBUG_LOG("extract_tool_calls: tool call extracted successfully");
            break;  // Only handle first tool call
        }
    }
}

res_t ik_repl_completion_callback(const ik_provider_completion_t *completion, void *ctx)
{
    DEBUG_LOG("repl_completion_callback: ENTRY completion=%p ctx=%p", (const void *)completion, ctx);

    assert(completion != NULL);  /* LCOV_EXCL_BR_LINE */
    assert(ctx != NULL);         /* LCOV_EXCL_BR_LINE */

    ik_agent_ctx_t *agent = (ik_agent_ctx_t *)ctx;

    DEBUG_LOG("repl_completion_callback: agent=%p success=%d response=%p",
              (void *)agent, completion->success, (const void *)completion->response);

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

    // Flush any remaining buffered line content (with prefix if first line)
    bool had_response_content = (agent->assistant_response != NULL);
    if (agent->streaming_line_buffer != NULL) {
        size_t buffer_len = strlen(agent->streaming_line_buffer);
        const char *model_prefix = ik_output_prefix(IK_OUTPUT_MODEL_TEXT);
        if (agent->streaming_first_line && model_prefix) {
            size_t prefix_len = strlen(model_prefix);
            size_t total_len = prefix_len + 1 + buffer_len;
            char *line = talloc_size(agent, total_len + 1);
            if (line == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
            memcpy(line, model_prefix, prefix_len);
            line[prefix_len] = ' ';
            memcpy(line + prefix_len + 1, agent->streaming_line_buffer, buffer_len);
            line[total_len] = '\0';
            ik_scrollback_append_line(agent->scrollback, line, total_len);
            talloc_free(line);
        } else {
            ik_scrollback_append_line(agent->scrollback, agent->streaming_line_buffer, buffer_len);
        }
        talloc_free(agent->streaming_line_buffer);
        agent->streaming_line_buffer = NULL;
        agent->streaming_first_line = false;
    }

    // Add blank line after response content (before usage line)
    if (had_response_content) {
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
        DEBUG_LOG("repl_completion_callback: calling store_response_metadata");
        store_response_metadata(agent, completion->response);
        DEBUG_LOG("repl_completion_callback: calling render_usage_event");
        render_usage_event(agent);
        DEBUG_LOG("repl_completion_callback: calling extract_tool_calls");
        extract_tool_calls(agent, completion->response);
        DEBUG_LOG("repl_completion_callback: extract_tool_calls returned");
    }

    DEBUG_LOG("repl_completion_callback: EXIT");
    return OK(NULL);
}
