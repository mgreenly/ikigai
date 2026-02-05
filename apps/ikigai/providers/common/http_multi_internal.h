#pragma once

#include "apps/ikigai/providers/common/http_multi.h"

#include <curl/curl.h>
#include <stddef.h>

/**
 * Write callback context
 *
 * Accumulates response data as it arrives.
 */
typedef struct {
    ik_http_write_cb_t user_callback;  /* User's streaming callback (or NULL) */
    void *user_ctx;                    /* Context for user callback */
    char *response_buffer;             /* Accumulated response data */
    size_t response_len;               /* Current response length */
    size_t buffer_capacity;            /* Allocated buffer size */
} http_write_ctx_t;

/**
 * Active request context
 *
 * Tracks state for a single in-flight HTTP request.
 */
typedef struct {
    CURL *easy_handle;                      /* curl easy handle for this request */
    struct curl_slist *headers;             /* HTTP headers */
    http_write_ctx_t *write_ctx;            /* Write callback context */
    char *request_body;                     /* Request body (must persist) */
    ik_http_completion_cb_t completion_cb;  /* Completion callback (or NULL) */
    void *completion_ctx;                   /* Context for completion callback */
} active_request_t;

/**
 * Multi-handle manager structure
 */
struct ik_http_multi {
    CURLM *multi_handle;                /* curl multi handle */
    active_request_t **active_requests; /* Array of active request contexts */
    size_t active_count;                /* Number of active requests */
};
