/**
 * @file http_server.h
 * @brief Minimal HTTP server for mock provider
 *
 * Parses HTTP requests from raw socket data and writes HTTP responses.
 * Routes requests to handler functions based on path.
 */

#ifndef MOCK_HTTP_SERVER_H
#define MOCK_HTTP_SERVER_H

#include <stddef.h>
#include <stdint.h>
#include <talloc.h>

/** Parsed HTTP request */
typedef struct {
    char *method;
    char *path;
    char *body;
    size_t body_len;
} http_request_t;

/**
 * Parse an HTTP request from raw data.
 * Returns NULL on parse error.
 */
http_request_t *http_request_parse(TALLOC_CTX *ctx, const char *data,
                                   size_t data_len);

/**
 * Write an HTTP response with JSON body.
 */
void http_respond_json(int fd, int status_code, const char *body);

/**
 * Write HTTP SSE response headers (200 OK, text/event-stream).
 * After this, write SSE data lines directly to fd.
 */
void http_respond_sse_start(int fd);

/**
 * Write an HTTP error response.
 */
void http_respond_error(int fd, int status_code, const char *message);

#endif /* MOCK_HTTP_SERVER_H */
