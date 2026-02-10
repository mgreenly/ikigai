/**
 * @file streaming.h
 * @brief Google streaming implementation (internal)
 *
 * Async streaming for Google Gemini API that integrates with select()-based event loop.
 * Parses Google SSE events and emits normalized ik_stream_event_t events.
 */

#ifndef IK_PROVIDERS_GOOGLE_STREAMING_H
#define IK_PROVIDERS_GOOGLE_STREAMING_H

#include <talloc.h>
#include "shared/error.h"
#include "apps/ikigai/providers/provider.h"

/**
 * Google streaming context
 *
 * Tracks streaming state, accumulated metadata, and user callbacks.
 * Created per streaming request.
 */
typedef struct ik_google_stream_ctx ik_google_stream_ctx_t;

/**
 * Create streaming context
 *
 * @param ctx        Talloc context for allocation
 * @param cb         Stream event callback
 * @param cb_ctx     User context for stream callback
 * @param out_stream_ctx Output: streaming context
 * @return           OK with context, ERR on failure
 *
 * Initializes:
 * - Stream callback and context
 * - State tracking (finish_reason, usage, model)
 * - Content block state (started, in_thinking, in_tool_call)
 * - part_index = 0
 * - finish_reason = IK_FINISH_UNKNOWN
 * - usage = all zeros
 */
res_t ik_google_stream_ctx_create(TALLOC_CTX *ctx,
                                  ik_stream_cb_t cb,
                                  void *cb_ctx,
                                  ik_google_stream_ctx_t **out_stream_ctx);

/**
 * Process single SSE data chunk from Google API
 *
 * @param stream_ctx Streaming context
 * @param data       Event data (JSON string)
 *
 * Parses Google SSE data chunks and emits normalized ik_stream_event_t events
 * via the stream callback.
 *
 * Called from curl write callback during perform() as data arrives.
 *
 * Data processing:
 * - Skip empty data strings
 * - Parse JSON chunk (silently ignore malformed JSON)
 * - Check for error object, emit IK_STREAM_ERROR if present
 * - Extract modelVersion on first chunk, emit IK_STREAM_START
 * - Process candidates[0].content.parts[] array
 * - Extract finishReason and usageMetadata from final chunk
 *
 * Part processing:
 * - functionCall: Generate tool ID, emit IK_STREAM_TOOL_CALL_START/DELTA
 * - thought=true: Emit IK_STREAM_THINKING_DELTA
 * - text: Emit IK_STREAM_TEXT_DELTA
 *
 * Finalization:
 * - usageMetadata present: End open tool calls, emit IK_STREAM_DONE
 */
void ik_google_stream_process_data(ik_google_stream_ctx_t *stream_ctx, const char *data);

/**
 * Get accumulated usage statistics
 *
 * @param stream_ctx Streaming context
 * @return           Usage statistics
 *
 * Returns accumulated token counts from final chunk with usageMetadata:
 * - input_tokens from promptTokenCount
 * - output_tokens = candidatesTokenCount - thoughtsTokenCount
 * - thinking_tokens from thoughtsTokenCount
 * - total_tokens from totalTokenCount
 */
ik_usage_t ik_google_stream_get_usage(ik_google_stream_ctx_t *stream_ctx);

/**
 * Build response from accumulated streaming data
 *
 * @param ctx        Talloc context for response allocation
 * @param stream_ctx Streaming context with accumulated data
 * @return           Response structure (allocated on ctx)
 *
 * Builds an ik_response_t from the accumulated streaming data including:
 * - model (from modelVersion)
 * - finish_reason (from finishReason)
 * - usage (from usageMetadata)
 * - content_blocks with tool call if present (from current_tool_*)
 *
 * Used by completion callback to provide a consistent response
 * for both streaming and non-streaming requests.
 */
ik_response_t *ik_google_stream_build_response(TALLOC_CTX *ctx, ik_google_stream_ctx_t *stream_ctx);

#endif /* IK_PROVIDERS_GOOGLE_STREAMING_H */
