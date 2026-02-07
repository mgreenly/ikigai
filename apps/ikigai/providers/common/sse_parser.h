#ifndef IK_PROVIDERS_COMMON_SSE_PARSER_H
#define IK_PROVIDERS_COMMON_SSE_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <talloc.h>

/**
 * SSE (Server-Sent Events) parser module
 *
 * Provides parsing functionality for Server-Sent Events streams.
 * Shared by all provider implementations (Anthropic, OpenAI, etc.).
 *
 * Memory: All structures use talloc-based ownership
 * API: Pull-based - caller loops over ik_sse_parser_next()
 */

/**
 * SSE parser state
 *
 * Accumulates incoming bytes and extracts complete SSE events.
 * Events are delimited by double newline (\n\n).
 */
typedef struct {
    char *buffer;    /* Accumulation buffer */
    size_t len;      /* Current buffer length */
    size_t cap;      /* Buffer capacity */
} ik_sse_parser_t;

/**
 * Parsed SSE event
 *
 * Represents a single Server-Sent Event with optional event type and data.
 */
typedef struct {
    char *event;     /* Event type (nullable) - e.g., "message" */
    char *data;      /* Event data (nullable) - payload content */
} ik_sse_event_t;

/**
 * Create a new SSE parser
 *
 * Allocates parser with initial buffer capacity (4096 bytes).
 * Panics on out-of-memory.
 *
 * @param parent  Talloc context parent (or NULL)
 * @return        Parser instance
 */
ik_sse_parser_t *ik_sse_parser_create(void *parent);

/**
 * Feed data to the SSE parser
 *
 * Appends incoming bytes to internal buffer.
 * Grows buffer exponentially if needed (doubles capacity).
 * Data does not need to be null-terminated.
 * No parsing happens during feed.
 * Panics on out-of-memory.
 *
 * @param parser  Parser instance
 * @param data    Data to feed (does not need to be null-terminated)
 * @param len     Length of data in bytes
 */
void ik_sse_parser_feed(ik_sse_parser_t *parser, const char *data, size_t len);

/**
 * Extract next complete SSE event
 *
 * Searches for \n\n delimiter in buffer.
 * If found, extracts event text, parses it line-by-line, and removes from buffer.
 * Returns parsed event allocated on provided context.
 * Returns NULL if no complete event available.
 *
 * Event parsing:
 * - Lines starting with "event: " set event type
 * - Lines starting with "data: " set/append data payload
 * - Multiple data lines concatenated with newlines
 * - Lines with "data:" (no space) treated as empty data
 *
 * @param parser  Parser instance
 * @param ctx     Context for allocating returned event
 * @return        Parsed event if available, NULL if no complete event
 */
ik_sse_event_t *ik_sse_parser_next(ik_sse_parser_t *parser, TALLOC_CTX *ctx);

/**
 * Check if event is [DONE] marker
 *
 * Tests if event data equals "[DONE]" string (case-sensitive exact match).
 * Signals end of stream.
 *
 * @param event  Event to check
 * @return       true if [DONE] marker, false otherwise
 */
bool ik_sse_event_is_done(const ik_sse_event_t *event);

#endif /* IK_PROVIDERS_COMMON_SSE_PARSER_H */
