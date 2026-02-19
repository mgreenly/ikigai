/**
 * @file anthropic_serializer.h
 * @brief Anthropic Messages API SSE serializer
 *
 * Converts mock queue responses into Anthropic-compatible SSE events
 * written directly to a socket fd.
 */

#ifndef MOCK_ANTHROPIC_SERIALIZER_H
#define MOCK_ANTHROPIC_SERIALIZER_H

#include "apps/mock-provider/mock_queue.h"

#include <stddef.h>
#include <stdint.h>
#include <talloc.h>

/**
 * Stream a text response as Anthropic Messages API SSE events.
 * Writes: message_start, content_block_start, content_block_delta,
 * content_block_stop, message_delta, message_stop.
 */
void anthropic_serialize_text(TALLOC_CTX *ctx, const char *content, int fd);

/**
 * Stream tool calls as Anthropic Messages API SSE events.
 * Writes: message_start, then for each tool call: content_block_start,
 * content_block_delta, content_block_stop, then message_delta, message_stop.
 */
void anthropic_serialize_tool_calls(TALLOC_CTX *ctx,
                                    const mock_tool_call_t *tool_calls,
                                    int32_t count, int fd);

#endif /* MOCK_ANTHROPIC_SERIALIZER_H */
