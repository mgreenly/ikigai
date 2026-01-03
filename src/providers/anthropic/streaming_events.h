/**
 * @file streaming_events.h
 * @brief Anthropic SSE event processors (internal)
 */

#ifndef IK_PROVIDERS_ANTHROPIC_STREAMING_EVENTS_H
#define IK_PROVIDERS_ANTHROPIC_STREAMING_EVENTS_H

#include "streaming.h"
#include "vendor/yyjson/yyjson.h"

/**
 * Process message_start event
 */
void ik_anthropic_process_message_start(ik_anthropic_stream_ctx_t *sctx, yyjson_val *root);

/**
 * Process content_block_start event
 */
void ik_anthropic_process_content_block_start(ik_anthropic_stream_ctx_t *sctx, yyjson_val *root);

/**
 * Process content_block_delta event
 */
void ik_anthropic_process_content_block_delta(ik_anthropic_stream_ctx_t *sctx, yyjson_val *root);

/**
 * Process content_block_stop event
 */
void ik_anthropic_process_content_block_stop(ik_anthropic_stream_ctx_t *sctx, yyjson_val *root);

/**
 * Process message_delta event
 */
void ik_anthropic_process_message_delta(ik_anthropic_stream_ctx_t *sctx, yyjson_val *root);

/**
 * Process message_stop event
 */
void ik_anthropic_process_message_stop(ik_anthropic_stream_ctx_t *sctx, yyjson_val *root);

/**
 * Process error event
 */
void ik_anthropic_process_error(ik_anthropic_stream_ctx_t *sctx, yyjson_val *root);

#endif /* IK_PROVIDERS_ANTHROPIC_STREAMING_EVENTS_H */
