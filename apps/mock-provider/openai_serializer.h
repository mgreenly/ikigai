/**
 * @file openai_serializer.h
 * @brief OpenAI Chat Completions and Responses API SSE serializers
 *
 * Converts mock queue responses into OpenAI-compatible SSE chunks
 * written directly to a socket fd.
 */

#ifndef MOCK_OPENAI_SERIALIZER_H
#define MOCK_OPENAI_SERIALIZER_H

#include "apps/mock-provider/mock_queue.h"

#include <stddef.h>
#include <stdint.h>
#include <talloc.h>

/* ================================================================
 * Chat Completions API (/v1/chat/completions)
 * ================================================================ */

/**
 * Stream a text response as OpenAI Chat Completions SSE chunks.
 * Writes: role chunk, content chunk, stop chunk, [DONE] sentinel.
 */
void openai_serialize_text(TALLOC_CTX *ctx, const char *content, int fd);

/**
 * Stream tool calls as OpenAI Chat Completions SSE chunks.
 * Writes: role chunk with tool_calls delta for each call, stop chunk,
 * [DONE] sentinel.
 */
void openai_serialize_tool_calls(TALLOC_CTX *ctx,
                                 const mock_tool_call_t *tool_calls,
                                 int32_t count, int fd);

/* ================================================================
 * Responses API (/v1/responses)
 * ================================================================ */

/**
 * Stream a text response as OpenAI Responses API SSE events.
 * Writes: response.created, response.output_text.delta,
 * response.completed events.
 */
void openai_responses_serialize_text(TALLOC_CTX *ctx, const char *content,
                                     int fd);

/**
 * Stream tool calls as OpenAI Responses API SSE events.
 * Writes: response.created, response.output_item.added,
 * response.function_call_arguments.delta,
 * response.output_item.done, response.completed events.
 */
void openai_responses_serialize_tool_calls(TALLOC_CTX *ctx,
                                           const mock_tool_call_t *tool_calls,
                                           int32_t count, int fd);

#endif /* MOCK_OPENAI_SERIALIZER_H */
