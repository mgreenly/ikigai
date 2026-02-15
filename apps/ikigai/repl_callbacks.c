// REPL HTTP callback handlers implementation
#include "apps/ikigai/repl_callbacks.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/ansi.h"
#include "apps/ikigai/debug_log.h"
#include "apps/ikigai/event_render.h"
#include "shared/logger.h"
#include "apps/ikigai/output_style.h"
#include "shared/panic.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/repl_actions.h"
#include "apps/ikigai/repl_response_helpers.h"
#include "apps/ikigai/shared.h"
#include "vendor/yyjson/yyjson.h"

#include <assert.h>
#include <string.h>
#include <talloc.h>


#include "shared/poison.h"
res_t ik_repl_stream_callback(const ik_stream_event_t *event, void *ctx)
{
    assert(event != NULL);  /* LCOV_EXCL_BR_LINE */
    assert(ctx != NULL);    /* LCOV_EXCL_BR_LINE */

    ik_agent_ctx_t *agent = (ik_agent_ctx_t *)ctx;

    DEBUG_LOG("stream_callback: event_type=%d", event->type);

    switch (event->type) { // LCOV_EXCL_BR_LINE - default case is defensive
        case IK_STREAM_START:
            DEBUG_LOG("stream_callback: IK_STREAM_START");
            if (agent->assistant_response != NULL) {
                talloc_free(agent->assistant_response);
                agent->assistant_response = NULL;
            }
            agent->streaming_first_line = true;
            break;

        case IK_STREAM_TEXT_DELTA:
            if (event->data.delta.text != NULL) {
                ik_repl_handle_text_delta(agent, event->data.delta.text, strlen(event->data.delta.text));
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

res_t ik_repl_completion_callback(const ik_provider_completion_t *completion, void *ctx)
{
    assert(completion != NULL);  /* LCOV_EXCL_BR_LINE */
    assert(ctx != NULL);         /* LCOV_EXCL_BR_LINE */

    DEBUG_LOG("completion_callback: ENTRY success=%d", completion->success);

    ik_agent_ctx_t *agent = (ik_agent_ctx_t *)ctx;

    DEBUG_LOG("completion_callback: logging response metadata");

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

    DEBUG_LOG("completion_callback: after logging");

    // Flush any remaining buffered line content (with prefix if first line)
    // Check for non-whitespace content (empty/whitespace-only responses are not displayed)
    bool had_response_content = false;
    if (agent->assistant_response != NULL) {
        for (const char *p = agent->assistant_response; *p != '\0'; p++) {
            if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
                had_response_content = true;
                break;
            }
        }
    }
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
        DEBUG_LOG("completion_callback: storing response metadata");
        ik_repl_store_response_metadata(agent, completion->response);
        DEBUG_LOG("completion_callback: rendering usage event");
        ik_repl_render_usage_event(agent);
        DEBUG_LOG("completion_callback: extracting tool calls");
        ik_repl_extract_tool_calls(agent, completion->response);
        DEBUG_LOG("completion_callback: tool calls extracted, pending=%p", (void *)agent->pending_tool_call);
    }

    DEBUG_LOG("completion_callback: EXIT");

    return OK(NULL);
}
