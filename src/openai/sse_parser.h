#ifndef IK_OPENAI_SSE_PARSER_H
#define IK_OPENAI_SSE_PARSER_H

#include "error.h"
#include <stddef.h>

/**
 * SSE (Server-Sent Events) parser module
 *
 * Provides parsing functionality for Server-Sent Events streams.
 * Used by the OpenAI client to handle streaming responses.
 *
 * Memory: All structures use talloc-based ownership
 * Errors: Result types with OK()/ERR() patterns
 */

/**
 * SSE parser state
 *
 * Accumulates incoming bytes and extracts complete SSE events.
 * Events are delimited by double newline (\n\n).
 */
typedef struct {
    char *buffer;         /* Accumulation buffer */
    size_t buffer_len;    /* Current buffer length (excluding null terminator) */
    size_t buffer_cap;    /* Buffer capacity (excluding null terminator) */
} ik_openai_sse_parser_t;

/**
 * Create a new SSE parser
 *
 * Panics on out-of-memory.
 *
 * @param parent  Talloc context parent (or NULL)
 * @return        Parser instance
 */
ik_openai_sse_parser_t *ik_openai_sse_parser_create(void *parent);

/**
 * Feed data to the SSE parser
 *
 * Accumulates incoming bytes into the internal buffer.
 * Call ik_openai_sse_parser_get_event() to extract complete events.
 *
 * Panics on out-of-memory.
 *
 * @param parser  Parser instance
 * @param data    Data to feed (does not need to be null-terminated)
 * @param len     Length of data in bytes
 */
void ik_openai_sse_parser_feed(ik_openai_sse_parser_t *parser,
                                const char *data, size_t len);

/**
 * Get the next complete SSE event from the parser
 *
 * Extracts and returns the next complete event (delimited by \n\n).
 * Removes the event from the internal buffer.
 *
 * Panics on out-of-memory.
 *
 * @param parser  Parser instance
 * @return        Event string if available, NULL if no complete event
 */
char *ik_openai_sse_parser_get_event(ik_openai_sse_parser_t *parser);

/**
 * Parse an SSE event and extract content delta
 *
 * Strips "data: " prefix, handles [DONE] marker, parses JSON,
 * and extracts choices[0].delta.content field.
 *
 * @param parent  Talloc context parent (or NULL)
 * @param event   SSE event string (e.g., "data: {...}")
 * @return        OK(content_string) if content present,
 *                OK(NULL) if [DONE] or no content,
 *                ERR(...) on parse error
 */
res_t ik_openai_parse_sse_event(void *parent, const char *event);

/*
 * Internal wrapper functions (exposed for testing)
 *
 * These consolidate yyjson inline functions to make defensive branches testable.
 */

#include "vendor/yyjson/yyjson.h"

yyjson_val *yyjson_doc_get_root_wrapper(yyjson_doc *doc);
yyjson_val *yyjson_arr_get_wrapper(yyjson_val *arr, size_t idx);
bool yyjson_is_obj_wrapper(yyjson_val *val);

#endif /* IK_OPENAI_SSE_PARSER_H */
