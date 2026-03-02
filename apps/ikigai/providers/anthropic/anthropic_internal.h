/**
 * @file anthropic_internal.h
 * @brief Internal types and functions for anthropic.c testing
 *
 * This header exposes internal types and callbacks for unit testing.
 * Do not include this in production code - only in anthropic.c and its tests.
 */

#ifndef IK_PROVIDERS_ANTHROPIC_INTERNAL_H
#define IK_PROVIDERS_ANTHROPIC_INTERNAL_H

#include <stddef.h>
#include <stdbool.h>
#include "apps/ikigai/providers/common/http_multi.h"
#include "apps/ikigai/providers/common/sse_parser.h"
#include "apps/ikigai/providers/anthropic/streaming.h"

/**
 * Active streaming context
 */
typedef struct ik_anthropic_active_stream {
    ik_anthropic_stream_ctx_t *stream_ctx;
    ik_sse_parser_t *sse_parser;
    ik_provider_completion_cb_t completion_cb;
    void *completion_ctx;
    bool completed;
    int http_status;
} ik_anthropic_active_stream_t;

/**
 * Anthropic provider implementation context
 *
 * Defined here (rather than in anthropic.c) so that count_tokens.c
 * and unit tests can access api_key and base_url without casting.
 */
typedef struct ik_anthropic_ctx {
    char *api_key;
    char *base_url;
    ik_http_multi_t *http_multi;
    ik_anthropic_active_stream_t *active_stream;
} ik_anthropic_ctx_t;

/**
 * HTTP write callback for streaming - exposed for testing
 */
size_t ik_anthropic_stream_write_cb(const char *data, size_t len, void *ctx);

/**
 * HTTP completion callback for streaming - exposed for testing
 */
void ik_anthropic_stream_completion_cb(const ik_http_completion_t *completion, void *ctx);

#endif /* IK_PROVIDERS_ANTHROPIC_INTERNAL_H */
