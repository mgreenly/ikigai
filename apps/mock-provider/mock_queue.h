/**
 * @file mock_queue.h
 * @brief FIFO response queue for mock provider
 *
 * Stores pre-scripted responses loaded via /_mock/expect.
 * Each pop returns the next response in order.
 */

#ifndef MOCK_QUEUE_H
#define MOCK_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <talloc.h>

/** Response types */
typedef enum {
    MOCK_TEXT,
    MOCK_TOOL_CALLS,
} mock_response_type_t;

/** A single tool call */
typedef struct {
    char *name;
    char *arguments_json;  /* serialized JSON string of arguments */
} mock_tool_call_t;

/** A queued response */
typedef struct mock_response {
    mock_response_type_t type;
    char *content;                /* for MOCK_TEXT */
    mock_tool_call_t *tool_calls; /* for MOCK_TOOL_CALLS */
    int32_t tool_call_count;
    struct mock_response *next;
} mock_response_t;

/** Opaque queue handle */
typedef struct mock_queue mock_queue_t;

/**
 * Create an empty response queue.
 */
mock_queue_t *mock_queue_create(TALLOC_CTX *ctx);

/**
 * Parse /_mock/expect JSON body and append responses to the queue.
 * Returns 0 on success, -1 on parse error.
 */
int mock_queue_load(mock_queue_t *queue, const char *json_body,
                    size_t json_len);

/**
 * Pop the next response from the queue.
 * Returns NULL if the queue is empty.
 * Caller must talloc_free the returned response when done.
 */
mock_response_t *mock_queue_pop(mock_queue_t *queue);

/**
 * Clear all responses from the queue.
 */
void mock_queue_reset(mock_queue_t *queue);

/**
 * Return true if the queue is empty.
 */
bool mock_queue_is_empty(const mock_queue_t *queue);

#endif /* MOCK_QUEUE_H */
