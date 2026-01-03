#ifndef GOOGLE_MOCK_VERIFICATION_HELPERS_H
#define GOOGLE_MOCK_VERIFICATION_HELPERS_H

#include <stdbool.h>
#include <stddef.h>
#include <talloc.h>

// SSE chunk accumulator
typedef struct {
    char **chunks;
    size_t count;
    size_t capacity;
} sse_accumulator_t;

// Check if verification mode is enabled
bool should_verify_mocks(void);

// Check if fixture capture mode is enabled
bool should_capture_fixtures(void);

// Get API key from environment or credentials file
const char *get_api_key_google(TALLOC_CTX *ctx);

// Create SSE accumulator
sse_accumulator_t *create_sse_accumulator(TALLOC_CTX *ctx);

// Add chunk to accumulator
void add_sse_chunk(sse_accumulator_t *acc, const char *chunk);

// Make HTTP POST request with SSE streaming
int http_post_sse_google(TALLOC_CTX *ctx, const char *url,
                         const char *body, sse_accumulator_t *acc);

// Make HTTP POST request (non-streaming)
int http_post_json_google(TALLOC_CTX *ctx, const char *url,
                          const char *body, char **out_response);

// Capture fixture to file
void capture_fixture_google(const char *name, sse_accumulator_t *acc);

#endif // GOOGLE_MOCK_VERIFICATION_HELPERS_H
