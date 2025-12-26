#ifndef ANTHROPIC_MOCK_VERIFICATION_HELPERS_H
#define ANTHROPIC_MOCK_VERIFICATION_HELPERS_H

#include <stdbool.h>
#include <stddef.h>
#include <talloc.h>

// SSE event accumulator
typedef struct {
    char **events;
    size_t count;
    size_t capacity;
} sse_event_accumulator_t;

// Check if verification mode is enabled
bool should_verify_mocks_anthropic(void);

// Check if fixture capture mode is enabled
bool should_capture_fixtures_anthropic(void);

// Get API key from environment or credentials file
const char *get_api_key_anthropic(TALLOC_CTX *ctx);

// Create SSE event accumulator
sse_event_accumulator_t *create_sse_event_accumulator(TALLOC_CTX *ctx);

// Add event to accumulator
void add_sse_event(sse_event_accumulator_t *acc, const char *event);

// Make HTTP POST request with SSE streaming
int http_post_sse_anthropic(TALLOC_CTX *ctx, const char *url, const char *api_key,
                            const char *body, sse_event_accumulator_t *acc);

// Make HTTP POST request (non-streaming)
int http_post_json_anthropic(TALLOC_CTX *ctx, const char *url, const char *api_key,
                             const char *body, char **out_response);

// Capture fixture to file
void capture_fixture_anthropic(const char *name, sse_event_accumulator_t *acc);

#endif // ANTHROPIC_MOCK_VERIFICATION_HELPERS_H
