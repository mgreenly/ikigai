#include "providers/common/sse_parser.h"
#include "error.h"
#include <talloc.h>
#include <string.h>
#include <assert.h>

/**
 * SSE parser implementation
 *
 * This module provides Server-Sent Events parsing functionality for streaming
 * HTTP responses. It accumulates incoming data and extracts complete events
 * delimited by double newlines (\n\n).
 *
 * Memory management:
 * - Parser owns internal buffer via talloc
 * - Events allocated on caller-provided context
 * - Buffer grows automatically, never shrinks
 * - All allocations cleaned up when parser freed
 */

#define SSE_INITIAL_BUFFER_SIZE 4096

ik_sse_parser_t *ik_sse_parser_create(void *parent) {
    ik_sse_parser_t *parser = talloc_zero(parent, ik_sse_parser_t);
    if (!parser) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate SSE parser"); // LCOV_EXCL_LINE
    }

    /* Allocate initial buffer */
    parser->buffer = talloc_array(parser, char, SSE_INITIAL_BUFFER_SIZE);
    if (!parser->buffer) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate SSE parser buffer"); // LCOV_EXCL_LINE
    }

    parser->buffer[0] = '\0';
    parser->len = 0;
    parser->cap = SSE_INITIAL_BUFFER_SIZE;

    return parser;
}

void ik_sse_parser_feed(ik_sse_parser_t *parser, const char *data, size_t len) {
    assert(parser != NULL); // LCOV_EXCL_BR_LINE
    assert(data != NULL || len == 0); // LCOV_EXCL_BR_LINE

    if (len == 0) {
        return;
    }

    /* Check if we need to grow the buffer */
    if (parser->len + len >= parser->cap) {
        /* Grow buffer to accommodate new data (double capacity until it fits) */
        size_t new_cap = parser->cap;
        while (new_cap < parser->len + len + 1) { /* +1 for null terminator */
            new_cap *= 2;
        }

        char *new_buffer = talloc_realloc(parser, parser->buffer, char, (unsigned int)new_cap);
        if (!new_buffer) { // LCOV_EXCL_BR_LINE
            PANIC("Failed to grow SSE parser buffer"); // LCOV_EXCL_LINE
        }

        parser->buffer = new_buffer;
        parser->cap = new_cap;
    }

    /* Append data to buffer */
    memcpy(parser->buffer + parser->len, data, len);
    parser->len += len;
    parser->buffer[parser->len] = '\0';
}

ik_sse_event_t *ik_sse_parser_next(ik_sse_parser_t *parser, TALLOC_CTX *ctx) {
    assert(parser != NULL); // LCOV_EXCL_BR_LINE
    assert(ctx != NULL); // LCOV_EXCL_BR_LINE

    /* Look for event delimiter: \n\n or \r\n\r\n */
    const char *delimiter = strstr(parser->buffer, "\n\n");
    const char *crlf_delimiter = strstr(parser->buffer, "\r\n\r\n");
    size_t delimiter_len = 2; /* Default for \n\n */

    /* Use whichever delimiter comes first */
    if (crlf_delimiter != NULL && (delimiter == NULL || crlf_delimiter < delimiter)) {
        delimiter = crlf_delimiter;
        delimiter_len = 4; /* \r\n\r\n */
    }

    if (!delimiter) {
        /* No complete event yet */
        return NULL;
    }

    /* Calculate event length (excluding the delimiter) */
    size_t event_len = (size_t)(delimiter - parser->buffer);

    /* Extract event text */
    char *event_text = talloc_strndup(ctx, parser->buffer, event_len);
    if (!event_text) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate event text"); // LCOV_EXCL_LINE
    }

    /* Remove event and delimiter from buffer */
    size_t consumed = event_len + delimiter_len;
    size_t remaining = parser->len - consumed;

    if (remaining > 0) {
        /* Move remaining data to start of buffer */
        memmove(parser->buffer, parser->buffer + consumed, remaining);
    }

    parser->len = remaining;
    parser->buffer[parser->len] = '\0';

    /* Parse event text line-by-line */
    ik_sse_event_t *event = talloc_zero(ctx, ik_sse_event_t);
    if (!event) { // LCOV_EXCL_BR_LINE
        PANIC("Failed to allocate SSE event"); // LCOV_EXCL_LINE
    }

    /* Track if we've seen any data lines */
    bool has_data = false;
    char *data_accum = NULL;
    size_t data_len = 0;

    /* Process lines */
    char *line = event_text;
    char *line_end;

    while (line && *line) {
        /* Find end of line */
        line_end = strchr(line, '\n');
        size_t current_line_len;
        if (line_end) {
            current_line_len = (size_t)(line_end - line);
        } else {
            current_line_len = strlen(line);
        }

        /* Process line based on prefix */
        if (current_line_len >= 6 && strncmp(line, "event:", 6) == 0) {
            /* Event type line - "event: <type>" or "event:<type>" */
            const char *event_start = line + 6;
            /* Skip optional space after colon */
            if (*event_start == ' ') {
                event_start++;
            }
            size_t event_type_len = current_line_len - (size_t)(event_start - line);
            event->event = talloc_strndup(event, event_start, event_type_len);
            if (!event->event) { // LCOV_EXCL_BR_LINE
                PANIC("Failed to allocate event type"); // LCOV_EXCL_LINE
            }
        } else if (current_line_len >= 5 && strncmp(line, "data:", 5) == 0) {
            /* Data line - "data: <content>" or "data:<content>" or "data:" (empty) */
            const char *data_start = line + 5;
            size_t content_len;

            /* Handle "data:" (no space, no content) as empty string */
            if (*data_start == ' ') {
                data_start++;
                content_len = current_line_len - 6; /* "data: " */
            } else {
                content_len = current_line_len - 5; /* "data:" */
            }

            /* Append to data accumulator */
            if (!has_data) {
                /* First data line */
                data_accum = talloc_strndup(event, data_start, content_len);
                if (!data_accum) { // LCOV_EXCL_BR_LINE
                    PANIC("Failed to allocate data accumulator"); // LCOV_EXCL_LINE
                }
                data_len = content_len;
                has_data = true;
            } else {
                /* Subsequent data line - concatenate with newline */
                size_t new_len = data_len + 1 + content_len;
                char *new_accum = talloc_realloc(event, data_accum, char, (unsigned int)(new_len + 1));
                if (!new_accum) { // LCOV_EXCL_BR_LINE
                    PANIC("Failed to grow data accumulator"); // LCOV_EXCL_LINE
                }
                new_accum[data_len] = '\n';
                memcpy(new_accum + data_len + 1, data_start, content_len);
                new_accum[new_len] = '\0';
                data_accum = new_accum;
                data_len = new_len;
            }
        }
        /* Ignore other line types (comments, etc.) */

        /* Move to next line */
        if (line_end) {
            line = line_end + 1;
        } else {
            break;
        }
    }

    /* Set final data field */
    event->data = data_accum;

    /* Free temporary event text */
    talloc_free(event_text);

    return event;
}

bool ik_sse_event_is_done(const ik_sse_event_t *event) {
    assert(event != NULL); // LCOV_EXCL_BR_LINE

    if (!event->data) {
        return false;
    }

    return strcmp(event->data, "[DONE]") == 0;
}

void ik_sse_parser_reset(ik_sse_parser_t *parser) {
    assert(parser != NULL); // LCOV_EXCL_BR_LINE

    parser->len = 0;
    parser->buffer[0] = '\0';
}
