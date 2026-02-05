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
#include "shared/error.h"
#include "apps/ikigai/providers/provider.h"

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
void ik_openai_chat_stream_process_data(ik_openai_chat_stream_ctx_t *stream_ctx, const char *data);

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

/**
 * Build response from accumulated streaming data
 *
 * @param ctx  Talloc context for allocation
 * @param sctx Streaming context with accumulated data
 * @return     Response structure (PANICs on OOM)
 *
 * Creates an ik_response_t by extracting accumulated data from the streaming context:
 * - model from sctx->model
 * - finish_reason from sctx->finish_reason
 * - usage from sctx->usage
 * - content_blocks with tool call if present (from current_tool_*)
 *
 * Call this after streaming completes to get a response suitable for the
 * completion callback.
 */
ik_response_t *ik_openai_chat_stream_build_response(TALLOC_CTX *ctx, ik_openai_chat_stream_ctx_t *sctx);

/**
 * OpenAI Responses API streaming context
 *
 * Tracks streaming state, accumulated metadata, and user callbacks.
 * Created per streaming request.
 */
typedef struct ik_openai_responses_stream_ctx ik_openai_responses_stream_ctx_t;

/**
 * Create Responses API streaming context
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
ik_openai_responses_stream_ctx_t *ik_openai_responses_stream_ctx_create(TALLOC_CTX *ctx,
                                                                        ik_stream_cb_t stream_cb,
                                                                        void *stream_ctx);

/**
 * Process single SSE event from OpenAI Responses API
 *
 * @param stream_ctx Streaming context
 * @param event_name SSE event name (e.g., "response.created", "response.output_text.delta")
 * @param data       Event data (JSON string)
 *
 * Parses OpenAI Responses API SSE events and emits normalized ik_stream_event_t events
 * via the stream callback.
 *
 * Event handling:
 * - response.created: Extract model, emit IK_STREAM_START
 * - response.output_text.delta: Emit IK_STREAM_TEXT_DELTA
 * - response.reasoning_summary_text.delta: Emit IK_STREAM_THINKING_DELTA
 * - response.output_item.added: Handle text/function_call items
 * - response.function_call_arguments.delta: Emit IK_STREAM_TOOL_CALL_DELTA
 * - response.output_item.done: Emit IK_STREAM_TOOL_CALL_DONE if tool
 * - response.completed: Extract usage, emit IK_STREAM_DONE with status mapping
 * - error: Parse error details, emit IK_STREAM_ERROR
 *
 * This function is called from the SSE parser callback during perform().
 */
void ik_openai_responses_stream_process_event(ik_openai_responses_stream_ctx_t *stream_ctx,
                                              const char *event_name,
                                              const char *data);

/**
 * Get finish reason from stream
 *
 * @param stream_ctx Streaming context
 * @return           Finish reason
 *
 * Returns finish reason extracted from response.status field.
 * IK_FINISH_UNKNOWN until status is provided.
 */
ik_finish_reason_t ik_openai_responses_stream_get_finish_reason(ik_openai_responses_stream_ctx_t *stream_ctx);

/**
 * Curl write callback for Responses API streaming
 *
 * @param ptr      Data chunk from curl
 * @param size     Size of each element
 * @param nmemb    Number of elements
 * @param userdata Streaming context (ik_openai_responses_stream_ctx_t*)
 * @return         Number of bytes consumed (size * nmemb to continue)
 *
 * Feeds incoming data to SSE parser. Called by curl during perform()
 * as data arrives. SSE parser invokes process_event for each complete event.
 */
size_t ik_openai_responses_stream_write_callback(void *ptr, size_t size, size_t nmemb, void *userdata);

/**
 * Build response from accumulated Responses API streaming data
 *
 * @param ctx  Talloc context for allocation
 * @param sctx Streaming context with accumulated data
 * @return     Response structure (PANICs on OOM)
 *
 * Creates an ik_response_t by extracting accumulated data from the streaming context:
 * - model from sctx->model
 * - finish_reason from sctx->finish_reason
 * - usage from sctx->usage
 * - content_blocks with tool call if present (from current_tool_*)
 *
 * Call this after streaming completes to get a response suitable for the
 * completion callback.
 */
ik_response_t *ik_openai_responses_stream_build_response(TALLOC_CTX *ctx, ik_openai_responses_stream_ctx_t *sctx);

#endif /* IK_PROVIDERS_OPENAI_STREAMING_H */
