/**
 * @file openai_streaming_responses_events_test_helpers.h
 * @brief Shared test helpers for OpenAI Responses API event tests
 */

#ifndef OPENAI_STREAMING_RESPONSES_EVENTS_TEST_HELPERS_H
#define OPENAI_STREAMING_RESPONSES_EVENTS_TEST_HELPERS_H

#include <talloc.h>
#include "providers/provider.h"
#include "error.h"

typedef struct {
    ik_stream_event_t *items;
    size_t count;
    size_t capacity;
} event_array_t;

extern TALLOC_CTX *test_ctx;
extern event_array_t *events;

res_t stream_cb(const ik_stream_event_t *event, void *ctx);
void setup(void);
void teardown(void);

#endif
