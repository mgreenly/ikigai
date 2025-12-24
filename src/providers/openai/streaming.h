/**
 * @file streaming.h
 * @brief OpenAI streaming implementation (internal)
 *
 * Async streaming for OpenAI Chat Completions API that integrates with select()-based event loop.
 * Parses OpenAI SSE events and emits normalized ik_stream_event_t events.
 */

#ifndef IK_PROVIDERS_OPENAI_STREAMING_H
#define IK_PROVIDERS_OPENAI_STREAMING_H

#include <talloc.h>
#include "error.h"
#include "providers/provider.h"

/**
 * OpenAI Chat Completions streaming context
 *
 * Tracks streaming state, accumulated metadata, and user callbacks.
 * Created per streaming request.
 */
typedef struct ik_openai_chat_stream_ctx ik_openai_chat_stream_ctx_t;

/**
 * Create Chat Completions streaming context
 *
 * @param ctx        Talloc context for allocation
 * @param stream_cb  Stream event callback
 * @param stream_ctx User context for stream callback
 * @return           Streaming context pointer (PANICs on OOM)
 *
 * Initializes:
 * - Stream callback and context
 * - State tracking (finish_reason, usage, started, in_tool_call)
 * - started = false
 * - in_tool_call = false
 * - tool_call_index = -1
 * - finish_reason = IK_FINISH_UNKNOWN
 * - usage = all zeros
 *
 * Note: Completion callback is NOT stored here. It is passed separately
 * to start_stream() and handled by the HTTP multi layer.
 */
ik_openai_chat_stream_ctx_t *ik_openai_chat_stream_ctx_create(TALLOC_CTX *ctx,
                                                                ik_stream_cb_t stream_cb,
                                                                void *stream_ctx);

/**
 * Process single SSE data event from OpenAI Chat Completions API
 *
 * @param stream_ctx Streaming context
 * @param data       Event data line (JSON string or "[DONE]")
 *
 * Parses OpenAI SSE data-only events and emits normalized ik_stream_event_t events
 * via the stream callback.
 *
 * Event handling:
 * - [DONE]: Emit IK_STREAM_DONE with final usage and finish_reason
 * - First delta: Extract model, emit IK_STREAM_START
 * - Content delta: Emit IK_STREAM_TEXT_DELTA
 * - Tool call delta: Track index, emit IK_STREAM_TOOL_CALL_START, DELTA, DONE
 * - Finish reason: Update finish_reason from choice
 * - Usage: Extract from final chunk (with stream_options.include_usage)
 * - Error: Parse error details, emit IK_STREAM_ERROR
 *
 * This function is called from the curl write callback during perform().
 */
void ik_openai_chat_stream_process_data(ik_openai_chat_stream_ctx_t *stream_ctx,
                                          const char *data);

/**
 * Get accumulated usage statistics
 *
 * @param stream_ctx Streaming context
 * @return           Usage statistics
 *
 * Returns accumulated token counts from final chunk:
 * - input_tokens (prompt_tokens)
 * - output_tokens (completion_tokens)
 * - thinking_tokens (completion_tokens_details.reasoning_tokens)
 * - total_tokens
 */
ik_usage_t ik_openai_chat_stream_get_usage(ik_openai_chat_stream_ctx_t *stream_ctx);

/**
 * Get finish reason from stream
 *
 * @param stream_ctx Streaming context
 * @return           Finish reason
 *
 * Returns finish reason extracted from choice.finish_reason field.
 * IK_FINISH_UNKNOWN until finish_reason is provided.
 */
ik_finish_reason_t ik_openai_chat_stream_get_finish_reason(ik_openai_chat_stream_ctx_t *stream_ctx);

#endif /* IK_PROVIDERS_OPENAI_STREAMING_H */
