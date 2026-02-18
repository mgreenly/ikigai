/**
 * @file http_server.c
 * @brief Minimal HTTP server for mock provider
 */

#include "apps/mock-provider/http_server.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

http_request_t *http_request_parse(TALLOC_CTX *ctx, const char *data,
                                   size_t data_len)
{
    assert(ctx != NULL);
    assert(data != NULL);

    if (data_len == 0) {
        return NULL;
    }

    /* Find end of request line */
    const char *line_end = memchr(data, '\r', data_len);
    if (line_end == NULL) {
        line_end = memchr(data, '\n', data_len);
    }
    if (line_end == NULL) {
        return NULL;
    }

    /* Parse method */
    const char *method_end = memchr(data, ' ', (size_t)(line_end - data));
    if (method_end == NULL) {
        return NULL;
    }

    /* Parse path */
    const char *path_start = method_end + 1;
    const char *path_end = memchr(path_start, ' ',
                                  (size_t)(line_end - path_start));
    if (path_end == NULL) {
        return NULL;
    }

    http_request_t *req = talloc_zero(ctx, http_request_t);
    if (req == NULL) {
        return NULL;
    }

    req->method = talloc_strndup(req, data, (size_t)(method_end - data));
    req->path = talloc_strndup(req, path_start,
                               (size_t)(path_end - path_start));

    /* Find Content-Length header */
    size_t content_length = 0;
    const char *header_start = line_end;

    while (header_start < data + data_len) {
        /* Skip CRLF or LF */
        if (*header_start == '\r') header_start++;
        if (header_start < data + data_len && *header_start == '\n') {
            header_start++;
        }

        /* Check for empty line (end of headers) */
        if (header_start < data + data_len &&
            (*header_start == '\r' || *header_start == '\n')) {
            /* Skip the empty line */
            if (*header_start == '\r') header_start++;
            if (header_start < data + data_len &&
                *header_start == '\n') {
                header_start++;
            }
            break;
        }

        /* Look for Content-Length header (case-insensitive) */
        if (strncasecmp(header_start, "Content-Length:",
                        strlen("Content-Length:")) == 0) {
            const char *val = header_start + strlen("Content-Length:");
            while (*val == ' ') val++;
            content_length = (size_t)strtoul(val, NULL, 10);
        }

        /* Skip to next line */
        const char *next_line = memchr(header_start, '\n',
                                       (size_t)(data + data_len -
                                                header_start));
        if (next_line == NULL) break;
        header_start = next_line;
    }

    /* Extract body */
    if (content_length > 0 && header_start < data + data_len) {
        size_t remaining = (size_t)(data + data_len - header_start);
        size_t body_len = content_length < remaining ? content_length
                                                     : remaining;
        req->body = talloc_strndup(req, header_start, body_len);
        req->body_len = body_len;
    } else if (header_start < data + data_len) {
        /* No Content-Length: use whatever remains after headers */
        size_t remaining = (size_t)(data + data_len - header_start);
        if (remaining > 0) {
            req->body = talloc_strndup(req, header_start, remaining);
            req->body_len = remaining;
        }
    }

    return req;
}

void http_respond_json(int fd, int status_code, const char *body)
{
    assert(fd >= 0);
    assert(body != NULL);

    const char *status_text = "OK";
    if (status_code == 400) status_text = "Bad Request";
    else if (status_code == 404) status_text = "Not Found";
    else if (status_code == 500) status_text = "Internal Server Error";
    else if (status_code == 503) status_text = "Service Unavailable";

    char header[256];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        status_code, status_text, strlen(body));

    if (header_len > 0) {
        (void)write(fd, header, (size_t)header_len);
    }
    (void)write(fd, body, strlen(body));
}

void http_respond_sse_start(int fd)
{
    assert(fd >= 0);

    const char *header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n"
        "\r\n";

    (void)write(fd, header, strlen(header));
}

void http_respond_error(int fd, int status_code, const char *message)
{
    assert(fd >= 0);
    assert(message != NULL);

    char body[512];
    int body_len = snprintf(body, sizeof(body),
                            "{\"error\":\"%s\"}", message);
    if (body_len < 0) body_len = 0;

    http_respond_json(fd, status_code, body);
}
