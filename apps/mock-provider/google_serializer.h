/**
 * @file google_serializer.h
 * @brief Google Gemini API SSE serializer
 *
 * Converts mock queue responses into Google-compatible SSE events
 * written directly to a socket fd.
 */

#ifndef MOCK_GOOGLE_SERIALIZER_H
#define MOCK_GOOGLE_SERIALIZER_H

#include "apps/mock-provider/mock_queue.h"

#include <stddef.h>
#include <stdint.h>
#include <talloc.h>

/**
 * Stream a text response as Google Gemini API SSE data.
 * Writes: data: {candidates with text part}\n\n
 */
void google_serialize_text(TALLOC_CTX *ctx, const char *content, int fd);

/**
 * Stream tool calls as Google Gemini API SSE data.
 * Writes: data: {candidates with functionCall parts}\n\n
 */
void google_serialize_tool_calls(TALLOC_CTX *ctx,
                                 const mock_tool_call_t *tool_calls,
                                 int32_t count, int fd);

#endif /* MOCK_GOOGLE_SERIALIZER_H */
