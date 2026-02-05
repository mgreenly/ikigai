// REPL response processing helper functions
#include "apps/ikigai/repl_response_helpers.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/ansi.h"
#include "apps/ikigai/debug_log.h"
#include "apps/ikigai/event_render.h"
#include "apps/ikigai/output_style.h"
#include "shared/panic.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/tool.h"

#include <assert.h>
#include <string.h>
#include <talloc.h>


#include "shared/poison.h"
void ik_repl_flush_line_to_scrollback(ik_agent_ctx_t *agent, const char *chunk,
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

void ik_repl_handle_text_delta(ik_agent_ctx_t *agent, const char *chunk, size_t chunk_len)
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
            ik_repl_flush_line_to_scrollback(agent, chunk, start, i - start);
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

void ik_repl_render_usage_event(ik_agent_ctx_t *agent)
{
    int32_t total = agent->response_input_tokens + agent->response_output_tokens +
                    agent->response_thinking_tokens;
    if (total > 0) {
        char data_json[256];
        snprintf(data_json, sizeof(data_json),
                 "{\"input_tokens\":%d,\"output_tokens\":%d,\"thinking_tokens\":%d}",
                 agent->response_input_tokens, agent->response_output_tokens,
                 agent->response_thinking_tokens);
        ik_event_render(agent->scrollback, "usage", NULL, data_json, false);
    } else {
        ik_scrollback_append_line(agent->scrollback, "", 0);
    }
}

void ik_repl_store_response_metadata(ik_agent_ctx_t *agent, const ik_response_t *response)
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

static void clear_pending_data(ik_agent_ctx_t *agent)
{
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
    if (agent->pending_tool_call != NULL) {
        talloc_free(agent->pending_tool_call);
        agent->pending_tool_call = NULL;
    }
    if (agent->pending_tool_thought_signature != NULL) {
        talloc_free(agent->pending_tool_thought_signature);
        agent->pending_tool_thought_signature = NULL;
    }
}

static void process_thinking_block(ik_agent_ctx_t *agent, ik_content_block_t *block)
{
    if (block->data.thinking.text != NULL) {
        agent->pending_thinking_text = talloc_strdup(agent, block->data.thinking.text);
        if (agent->pending_thinking_text == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }
    if (block->data.thinking.signature != NULL) {
        agent->pending_thinking_signature = talloc_strdup(agent, block->data.thinking.signature);
        if (agent->pending_thinking_signature == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }
}

static void process_redacted_thinking_block(ik_agent_ctx_t *agent, ik_content_block_t *block)
{
    if (block->data.redacted_thinking.data != NULL) {
        agent->pending_redacted_data = talloc_strdup(agent, block->data.redacted_thinking.data);
        if (agent->pending_redacted_data == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }
}

static bool process_tool_call_block(ik_agent_ctx_t *agent, ik_content_block_t *block)
{
    DEBUG_LOG("process_tool_call_block: id=%s name=%s args=%s",
              block->data.tool_call.id ? block->data.tool_call.id : "(NULL)",
              block->data.tool_call.name ? block->data.tool_call.name : "(NULL)",
              block->data.tool_call.arguments ? "(has args)" : "(NULL)");

    agent->pending_tool_call = ik_tool_call_create(agent,
                                                   block->data.tool_call.id,
                                                   block->data.tool_call.name,
                                                   block->data.tool_call.arguments);
    DEBUG_LOG("process_tool_call_block: ik_tool_call_create returned %p", (void *)agent->pending_tool_call);
    DEBUG_LOG("process_tool_call_block: about to check NULL");
    if (agent->pending_tool_call == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    DEBUG_LOG("process_tool_call_block: passed NULL check");

    DEBUG_LOG("process_tool_call_block: checking thought_signature=%p",
              (void *)block->data.tool_call.thought_signature);
    if (block->data.tool_call.thought_signature != NULL) {
        DEBUG_LOG("process_tool_call_block: copying thought_signature");
        agent->pending_tool_thought_signature = talloc_strdup(agent, block->data.tool_call.thought_signature);
        if (agent->pending_tool_thought_signature == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }
    DEBUG_LOG("process_tool_call_block: returning true");
    return true;  // Indicates we should stop processing
}

void ik_repl_extract_tool_calls(ik_agent_ctx_t *agent, const ik_response_t *response)
{
    DEBUG_LOG("extract_tool_calls: ENTRY content_count=%zu", response->content_count);
    clear_pending_data(agent);
    DEBUG_LOG("extract_tool_calls: cleared pending data");

    for (size_t i = 0; i < response->content_count; i++) {
        ik_content_block_t *block = &response->content_blocks[i];
        DEBUG_LOG("extract_tool_calls: block[%zu] type=%d", i, block->type);

        if (block->type == IK_CONTENT_THINKING) {
            process_thinking_block(agent, block);
        } else if (block->type == IK_CONTENT_REDACTED_THINKING) {
            process_redacted_thinking_block(agent, block);
        } else if (block->type == IK_CONTENT_TOOL_CALL) {
            DEBUG_LOG("extract_tool_calls: processing tool call name=%s", block->data.tool_call.name);
            if (process_tool_call_block(agent, block)) {
                DEBUG_LOG("extract_tool_calls: tool call processed, breaking");
                break;  // Only handle first tool call
            }
        }
    }
    DEBUG_LOG("extract_tool_calls: EXIT pending_tool_call=%p", (void *)agent->pending_tool_call);
}
