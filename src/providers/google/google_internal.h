/**
 * @file google_internal.h
 * @brief Internal types and functions for google.c testing
 *
 * This header exposes internal types and callbacks for unit testing.
 * Do not include this in production code - only in google.c and its tests.
 */

#ifndef IK_PROVIDERS_GOOGLE_INTERNAL_H
#define IK_PROVIDERS_GOOGLE_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "providers/common/http_multi.h"
#include "providers/common/sse_parser.h"
#include "streaming.h"

/**
 * Active streaming context
 */
typedef struct ik_google_active_stream {
    ik_google_stream_ctx_t *stream_ctx;
    ik_sse_parser_t *sse_parser;
    ik_provider_completion_cb_t completion_cb;
    void *completion_ctx;
    bool completed;
    int http_status;
} ik_google_active_stream_t;

/**
 * Google provider implementation context
 */
typedef struct ik_google_ctx {
    char *api_key;
    char *base_url;
    ik_http_multi_t *http_multi;
    ik_google_active_stream_t *active_stream;
} ik_google_ctx_t;

/**
 * HTTP write callback for streaming - exposed for testing
 */
size_t ik_google_stream_write_cb(const char *data, size_t len, void *ctx);

/**
 * HTTP completion callback for streaming - exposed for testing
 */
void ik_google_stream_completion_cb(const ik_http_completion_t *completion, void *ctx);

#endif /* IK_PROVIDERS_GOOGLE_INTERNAL_H */
