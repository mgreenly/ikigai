#ifndef IK_PROVIDERS_COMMON_HTTP_MULTI_H
#define IK_PROVIDERS_COMMON_HTTP_MULTI_H

#include "shared/error.h"
#include <sys/select.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

// Forward declaration
typedef struct ik_logger ik_logger_t;

/**
 * Shared HTTP multi-handle client for provider integrations
 *
 * Provides non-blocking HTTP client interface using curl_multi.
 * Integrates with select()-based event loops.
 * Generic layer for all provider adapters.
 *
 * Memory: talloc-based ownership
 * Errors: Result types with OK()/ERR() patterns
 */

/**
 * HTTP completion status types
 */
typedef enum {
    IK_HTTP_SUCCESS,           /* HTTP 200-299 */
    IK_HTTP_CLIENT_ERROR,      /* HTTP 400-499 (401 unauthorized, 429 rate limit, etc.) */
    IK_HTTP_SERVER_ERROR,      /* HTTP 500-599 */
    IK_HTTP_NETWORK_ERROR,     /* Connection failed, timeout, DNS error, etc. */
} ik_http_status_type_t;

/**
 * HTTP request specification
 *
 * Describes an HTTP request to send.
 */
typedef struct {
    const char *url;           /* Request URL (required) */
    const char *method;        /* HTTP method (GET, POST, etc.) */
    const char **headers;      /* NULL-terminated array of header strings (e.g., "Content-Type: application/json") */
    const char *body;          /* Request body (or NULL for GET) */
    size_t body_len;           /* Body length in bytes */
} ik_http_request_t;

/**
 * HTTP request completion information
 *
 * Provided to completion callback when a request finishes.
 */
typedef struct {
    ik_http_status_type_t type;  /* Completion status type */
    int32_t http_code;            /* HTTP response code (0 if network error) */
    int32_t curl_code;            /* CURLcode (CURLE_OK on success) */
    char *error_message;          /* Human-readable error message (or NULL on success) */
    char *response_body;          /* Raw response body bytes (or NULL) */
    size_t response_len;          /* Response body length in bytes */
} ik_http_completion_t;

/**
 * Write callback for streaming response data
 *
 * Called by curl as data arrives during perform().
 *
 * @param data  Chunk of response data
 * @param len   Length of data chunk in bytes
 * @param ctx   User-provided context pointer
 * @return      Number of bytes processed (must return len to continue)
 */
typedef size_t (*ik_http_write_cb_t)(const char *data, size_t len, void *ctx);

/**
 * Completion callback for finished requests
 *
 * Called by ik_http_multi_info_read() for each completed request.
 *
 * @param completion  Completion information (success or error)
 * @param ctx         User-provided context pointer
 */
typedef void (*ik_http_completion_cb_t)(const ik_http_completion_t *completion, void *ctx);

/**
 * Multi-handle manager structure (opaque)
 *
 * Manages non-blocking HTTP requests using curl_multi interface.
 * Integrates with select()-based event loops.
 */
typedef struct ik_http_multi ik_http_multi_t;

/**
 * Create a multi-handle manager
 *
 * @param parent  Talloc context parent (or NULL)
 * @return        OK(multi) or ERR(...)
 */
res_t ik_http_multi_create(void *parent);

/**
 * Add a request to the multi-handle (non-blocking)
 *
 * Initiates an HTTP request without blocking. The request will make progress
 * when ik_http_multi_perform() is called.
 *
 * @param multi          Multi-handle manager
 * @param req            HTTP request specification
 * @param write_cb       Callback for streaming chunks (or NULL)
 * @param write_ctx      Context pointer passed to write callback
 * @param completion_cb  Callback for request completion (or NULL)
 * @param completion_ctx Context pointer passed to completion callback
 * @return               OK(NULL) or ERR(...)
 */
res_t ik_http_multi_add_request(ik_http_multi_t *multi,
                                const ik_http_request_t *req,
                                ik_http_write_cb_t write_cb,
                                void *write_ctx,
                                ik_http_completion_cb_t completion_cb,
                                void *completion_ctx);

/**
 * Perform non-blocking I/O operations
 *
 * Call this when select() indicates curl FDs are ready, or periodically.
 *
 * @param multi          Multi-handle manager
 * @param still_running  Output: number of requests still in progress
 * @return               OK(NULL) or ERR(...)
 */
res_t ik_http_multi_perform(ik_http_multi_t *multi, int *still_running);

/**
 * Get file descriptors for select()
 *
 * Populates fd_sets with curl's file descriptors.
 *
 * @param multi      Multi-handle manager
 * @param read_fds   Read FD set (will be modified)
 * @param write_fds  Write FD set (will be modified)
 * @param exc_fds    Exception FD set (will be modified)
 * @param max_fd     Output: highest FD number + 1
 * @return           OK(NULL) or ERR(...)
 */
res_t ik_http_multi_fdset(ik_http_multi_t *multi, fd_set *read_fds, fd_set *write_fds, fd_set *exc_fds, int *max_fd);

/**
 * Get timeout for select()
 *
 * Returns the timeout value curl recommends for select().
 *
 * @param multi      Multi-handle manager
 * @param timeout_ms Output: timeout in milliseconds (-1 = no timeout)
 * @return           OK(NULL) or ERR(...)
 */
res_t ik_http_multi_timeout(ik_http_multi_t *multi, long *timeout_ms);

/**
 * Check for completed requests
 *
 * Call this after ik_http_multi_perform() to handle completed transfers.
 * Processes all completed requests and invokes completion callbacks.
 *
 * @param multi   Multi-handle manager
 * @param logger  Logger for debug output (or NULL)
 */
void ik_http_multi_info_read(ik_http_multi_t *multi, ik_logger_t *logger);

/**
 * Cancel all active requests
 *
 * Immediately removes all in-flight requests from the curl multi handle.
 * Does NOT invoke completion callbacks - caller is responsible for cleanup.
 *
 * @param multi  Multi-handle manager
 */
void ik_http_multi_cancel_all(ik_http_multi_t *multi);

#endif /* IK_PROVIDERS_COMMON_HTTP_MULTI_H */
