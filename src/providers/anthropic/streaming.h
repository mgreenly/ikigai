/**
 * @file streaming.h
 * @brief Anthropic streaming implementation (internal)
 *
 * Async streaming for Anthropic API that integrates with select()-based event loop.
 * Parses Anthropic SSE events and emits normalized ik_stream_event_t events.
 */

#ifndef IK_PROVIDERS_ANTHROPIC_STREAMING_H
#define IK_PROVIDERS_ANTHROPIC_STREAMING_H

#include <talloc.h>
#include "error.h"
#include "providers/provider.h"
#include "providers/common/sse_parser.h"

/**
 * Anthropic streaming context structure
 *
 * Tracks streaming state, accumulated metadata, and user callbacks.
 * Created per streaming request.
 */
struct ik_anthropic_stream_ctx {
    ik_stream_cb_t stream_cb;          /* User's stream callback */
    void *stream_ctx;                  /* User's stream context */
    ik_sse_parser_t *sse_parser;       /* SSE parser instance */
    char *model;                       /* Model name from message_start */
    ik_finish_reason_t finish_reason;  /* Finish reason from message_delta */
    ik_usage_t usage;                  /* Accumulated usage statistics */
    int32_t current_block_index;       /* Current content block index */
    ik_content_type_t current_block_type; /* Current block type */
    char *current_tool_id;             /* Current tool call ID */
    char *current_tool_name;           /* Current tool call name */
};

typedef struct ik_anthropic_stream_ctx ik_anthropic_stream_ctx_t;

/**
 * Create streaming context
 *
 * @param ctx        Talloc context for allocation
 * @param stream_cb  Stream event callback
 * @param stream_ctx User context for stream callback
 * @param out        Output: streaming context
 * @return           OK with context, ERR on failure
 *
 * Initializes:
 * - SSE parser with event processing callback
 * - Stream callback and context
 * - State tracking (finish_reason, usage, current_block_*)
 * - current_block_index = -1
 * - finish_reason = IK_FINISH_UNKNOWN
 * - usage = all zeros
 *
 * Note: Completion callback is NOT stored here. It is passed separately
 * to start_stream() and handled by the HTTP multi layer.
 */
res_t ik_anthropic_stream_ctx_create(TALLOC_CTX *ctx, ik_stream_cb_t stream_cb,
                                      void *stream_ctx, ik_anthropic_stream_ctx_t **out);

/**
 * Process single SSE event from Anthropic API
 *
 * @param stream_ctx Streaming context
 * @param event      Event type string (e.g., "message_start", "content_block_delta")
 * @param data       Event data (JSON string)
 *
 * Parses Anthropic SSE events and emits normalized ik_stream_event_t events
 * via the stream callback.
 *
 * Event handling:
 * - message_start: Extract model and initial usage, emit IK_STREAM_START
 * - content_block_start: Track block type/index, emit IK_STREAM_TOOL_CALL_START for tool_use
 * - content_block_delta: Emit IK_STREAM_TEXT_DELTA, IK_STREAM_THINKING_DELTA, or IK_STREAM_TOOL_CALL_DELTA
 * - content_block_stop: Emit IK_STREAM_TOOL_CALL_DONE for tool_use blocks
 * - message_delta: Update finish_reason and usage (no event emission)
 * - message_stop: Emit IK_STREAM_DONE with final usage and finish_reason
 * - ping: Ignore (keep-alive)
 * - error: Parse error details, emit IK_STREAM_ERROR
 *
 * This function is called by the SSE parser's event callback during curl write callbacks.
 */
void ik_anthropic_stream_process_event(ik_anthropic_stream_ctx_t *stream_ctx,
                                        const char *event, const char *data);

#endif /* IK_PROVIDERS_ANTHROPIC_STREAMING_H */
