/**
 * @file vcr.c
 * @brief VCR implementation - HTTP recording/replay for deterministic tests
 */

#include "vcr_helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// Global recording flag (exposed in header)
bool vcr_recording = false;

// Internal structures
typedef struct {
    char *method;
    char *url;
    char *headers;
    char *body;
} request_data_t;

typedef struct {
    char **chunks;
    size_t count;
    size_t index;
} chunk_queue_t;

typedef struct {
    FILE *fp;
    bool recording;
    char *fixture_path;
    request_data_t *recorded_request;
    chunk_queue_t *chunk_queue;
    bool skip_verification;
    int response_status;
} vcr_state_t;

// Global VCR state
static vcr_state_t *g_vcr_state = NULL;

// Forward declarations
static void parse_fixture(vcr_state_t *state);
static char *json_escape(const char *str);
static char *json_unescape(const char *str);
static const char *redact_credential_header(const char *name, const char *value);
static void free_request_data(request_data_t *req);
static void free_chunk_queue(chunk_queue_t *queue);

void vcr_init(const char *test_name, const char *provider)
{
    if (g_vcr_state != NULL) {
        fprintf(stderr, "VCR: Warning: vcr_init called twice, cleaning up previous state\n");
        vcr_finish();
    }

    g_vcr_state = calloc(1, sizeof(vcr_state_t));
    if (!g_vcr_state) {
        fprintf(stderr, "VCR: Failed to allocate state\n");
        return;
    }

    // Check recording mode
    const char *record_env = getenv("VCR_RECORD");
    g_vcr_state->recording = (record_env != NULL && strcmp(record_env, "1") == 0);
    vcr_recording = g_vcr_state->recording;

    // Build fixture path: tests/fixtures/vcr/{provider}/{test_name}.jsonl
    char path[512];
    snprintf(path, sizeof(path), "tests/fixtures/vcr/%s/%s.jsonl", provider, test_name);
    g_vcr_state->fixture_path = strdup(path);

    // Open file
    if (g_vcr_state->recording) {
        g_vcr_state->fp = fopen(path, "w");
        if (!g_vcr_state->fp) {
            fprintf(stderr, "VCR: Failed to open fixture for writing: %s\n", path);
        }
    } else {
        g_vcr_state->fp = fopen(path, "r");
        if (!g_vcr_state->fp) {
            fprintf(stderr, "VCR: Warning: Failed to open fixture for reading: %s\n", path);
        } else {
            parse_fixture(g_vcr_state);
        }
    }
}

void vcr_finish(void)
{
    if (!g_vcr_state) {
        return;
    }

    if (g_vcr_state->fp) {
        fclose(g_vcr_state->fp);
    }

    free(g_vcr_state->fixture_path);

    if (g_vcr_state->recorded_request) {
        free_request_data(g_vcr_state->recorded_request);
        free(g_vcr_state->recorded_request);
    }

    if (g_vcr_state->chunk_queue) {
        free_chunk_queue(g_vcr_state->chunk_queue);
        free(g_vcr_state->chunk_queue);
    }

    free(g_vcr_state);
    g_vcr_state = NULL;
    vcr_recording = false;
}

bool vcr_is_active(void)
{
    return g_vcr_state != NULL;
}

bool vcr_is_recording(void)
{
    return vcr_recording;
}

int vcr_get_response_status(void)
{
    if (!g_vcr_state) {
        return 0;
    }
    return g_vcr_state->response_status;
}

void vcr_skip_request_verification(void)
{
    if (g_vcr_state) {
        g_vcr_state->skip_verification = true;
    }
}

bool vcr_next_chunk(const char **data_out, size_t *len_out)
{
    if (!g_vcr_state || !g_vcr_state->chunk_queue) {
        return false;
    }

    chunk_queue_t *queue = g_vcr_state->chunk_queue;
    if (queue->index >= queue->count) {
        return false;
    }

    const char *chunk = queue->chunks[queue->index];
    queue->index++;

    *data_out = chunk;
    *len_out = strlen(chunk);
    return true;
}

bool vcr_has_more(void)
{
    if (!g_vcr_state || !g_vcr_state->chunk_queue) {
        return false;
    }

    chunk_queue_t *queue = g_vcr_state->chunk_queue;
    return queue->index < queue->count;
}

void vcr_record_request(const char *method, const char *url,
                       const char *headers, const char *body)
{
    if (!g_vcr_state || !g_vcr_state->recording || !g_vcr_state->fp) {
        return;
    }

    // Parse and redact headers
    char redacted_headers[4096] = "";
    if (headers && headers[0]) {
        char *headers_copy = strdup(headers);
        char *line = strtok(headers_copy, "\n");
        bool first = true;

        while (line) {
            // Skip empty lines
            if (line[0] == '\0') {
                line = strtok(NULL, "\n");
                continue;
            }

            // Parse header: "Name: Value"
            char *colon = strchr(line, ':');
            if (colon) {
                *colon = '\0';
                const char *name = line;
                const char *value = colon + 1;
                // Skip leading space in value
                while (*value == ' ') value++;

                const char *redacted_value = redact_credential_header(name, value);

                if (!first) {
                    strncat(redacted_headers, "\\n", sizeof(redacted_headers) - strlen(redacted_headers) - 1);
                }
                strncat(redacted_headers, name, sizeof(redacted_headers) - strlen(redacted_headers) - 1);
                strncat(redacted_headers, ": ", sizeof(redacted_headers) - strlen(redacted_headers) - 1);
                strncat(redacted_headers, redacted_value, sizeof(redacted_headers) - strlen(redacted_headers) - 1);
                first = false;
            }

            line = strtok(NULL, "\n");
        }
        free(headers_copy);
    }

    char *escaped_body = body ? json_escape(body) : NULL;

    fprintf(g_vcr_state->fp, "{\"_request\": {\"method\": \"%s\", \"url\": \"%s\", \"headers\": \"%s\"",
            method, url, redacted_headers);

    if (escaped_body) {
        fprintf(g_vcr_state->fp, ", \"body\": \"%s\"", escaped_body);
        free(escaped_body);
    }

    fprintf(g_vcr_state->fp, "}}\n");
    fflush(g_vcr_state->fp);
}

void vcr_record_response(int status, const char *headers)
{
    if (!g_vcr_state || !g_vcr_state->recording || !g_vcr_state->fp) {
        return;
    }

    // Escape headers for JSON
    char escaped_headers[4096] = "";
    if (headers && headers[0]) {
        char *headers_copy = strdup(headers);
        char *line = strtok(headers_copy, "\n");
        bool first = true;

        while (line) {
            if (line[0] == '\0') {
                line = strtok(NULL, "\n");
                continue;
            }

            if (!first) {
                strncat(escaped_headers, "\\n", sizeof(escaped_headers) - strlen(escaped_headers) - 1);
            }
            strncat(escaped_headers, line, sizeof(escaped_headers) - strlen(escaped_headers) - 1);
            first = false;

            line = strtok(NULL, "\n");
        }
        free(headers_copy);
    }

    fprintf(g_vcr_state->fp, "{\"_response\": {\"status\": %d, \"headers\": \"%s\"}}\n",
            status, escaped_headers);
    fflush(g_vcr_state->fp);
}

void vcr_record_chunk(const char *data, size_t len)
{
    if (!g_vcr_state || !g_vcr_state->recording || !g_vcr_state->fp) {
        return;
    }

    // Create null-terminated string from data
    char *chunk_str = malloc(len + 1);
    memcpy(chunk_str, data, len);
    chunk_str[len] = '\0';

    char *escaped = json_escape(chunk_str);
    fprintf(g_vcr_state->fp, "{\"_chunk\": \"%s\"}\n", escaped);
    fflush(g_vcr_state->fp);

    free(escaped);
    free(chunk_str);
}

void vcr_record_body(const char *data, size_t len)
{
    if (!g_vcr_state || !g_vcr_state->recording || !g_vcr_state->fp) {
        return;
    }

    // Create null-terminated string from data
    char *body_str = malloc(len + 1);
    memcpy(body_str, data, len);
    body_str[len] = '\0';

    char *escaped = json_escape(body_str);
    fprintf(g_vcr_state->fp, "{\"_body\": \"%s\"}\n", escaped);
    fflush(g_vcr_state->fp);

    free(escaped);
    free(body_str);
}

void vcr_verify_request(const char *method, const char *url, const char *body)
{
    if (!g_vcr_state || g_vcr_state->recording || g_vcr_state->skip_verification) {
        return;
    }

    if (!g_vcr_state->recorded_request) {
        fprintf(stderr, "VCR: Warning: No recorded request to verify against\n");
        return;
    }

    request_data_t *req = g_vcr_state->recorded_request;

    if (strcmp(req->method, method) != 0) {
        fprintf(stderr, "VCR: Warning: Method mismatch: expected '%s', got '%s'\n",
                req->method, method);
    }

    if (strcmp(req->url, url) != 0) {
        fprintf(stderr, "VCR: Warning: URL mismatch: expected '%s', got '%s'\n",
                req->url, url);
    }

    if (body && req->body) {
        if (strcmp(req->body, body) != 0) {
            fprintf(stderr, "VCR: Warning: Body mismatch\n");
        }
    } else if (body != req->body) {
        // One is NULL, the other isn't
        fprintf(stderr, "VCR: Warning: Body presence mismatch\n");
    }
}

// Internal helper functions

static const char *find_end_quote_with_escapes(const char *start)
{
    const char *p = start;
    while (*p) {
        if (*p == '"' && (p == start || *(p-1) != '\\')) {
            return p;
        }
        p++;
    }
    return NULL;
}

static char *extract_simple_string(const char *line, const char *key)
{
    size_t key_len = strlen(key);
    const char *start = strstr(line, key);
    if (!start) return NULL;

    start += key_len;
    const char *end = strchr(start, '"');
    if (!end) return NULL;

    size_t len = (size_t)(end - start);
    char *result = malloc(len + 1);
    memcpy(result, start, len);
    result[len] = '\0';
    return result;
}

static char *extract_and_unescape_string(const char *line, const char *key)
{
    size_t key_len = strlen(key);
    const char *start = strstr(line, key);
    if (!start) return NULL;

    start += key_len;
    const char *end = find_end_quote_with_escapes(start);
    if (!end) return NULL;

    size_t len = (size_t)(end - start);
    char *escaped = malloc(len + 1);
    memcpy(escaped, start, len);
    escaped[len] = '\0';

    char *unescaped = json_unescape(escaped);
    free(escaped);
    return unescaped;
}

static void add_to_chunk_queue(vcr_state_t *state, char *chunk)
{
    state->chunk_queue->count++;
    state->chunk_queue->chunks = realloc(state->chunk_queue->chunks,
                                         state->chunk_queue->count * sizeof(char*));
    state->chunk_queue->chunks[state->chunk_queue->count - 1] = chunk;
}

static void parse_request_line(vcr_state_t *state, const char *line)
{
    if (!state->recorded_request) {
        state->recorded_request = calloc(1, sizeof(request_data_t));
    }

    state->recorded_request->method = extract_simple_string(line, "\"method\": \"");
    state->recorded_request->url = extract_simple_string(line, "\"url\": \"");
    state->recorded_request->body = extract_and_unescape_string(line, "\"body\": \"");
}

static void parse_response_line(vcr_state_t *state, const char *line)
{
    const char *status_start = strstr(line, "\"status\": ");
    if (status_start) {
        state->response_status = atoi(status_start + 10);
    }
}

static void parse_chunk_line(vcr_state_t *state, const char *line)
{
    char *chunk = extract_and_unescape_string(line, "\"_chunk\": \"");
    if (chunk) {
        add_to_chunk_queue(state, chunk);
    }
}

static void parse_body_line(vcr_state_t *state, const char *line)
{
    char *body = extract_and_unescape_string(line, "\"_body\": \"");
    if (body) {
        add_to_chunk_queue(state, body);
    }
}

static void parse_fixture(vcr_state_t *state)
{
    if (!state->fp) {
        return;
    }

    char *line = NULL;
    size_t line_cap = 0;
    ssize_t line_len;

    state->chunk_queue = calloc(1, sizeof(chunk_queue_t));

    while ((line_len = getline(&line, &line_cap, state->fp)) > 0) {
        if (line[line_len - 1] == '\n') {
            line[line_len - 1] = '\0';
            line_len--;
        }

        if (strstr(line, "\"_request\"")) {
            parse_request_line(state, line);
        } else if (strstr(line, "\"_response\"")) {
            parse_response_line(state, line);
        } else if (strstr(line, "\"_chunk\"")) {
            parse_chunk_line(state, line);
        } else if (strstr(line, "\"_body\"")) {
            parse_body_line(state, line);
        }
    }

    free(line);
}

static char *json_escape(const char *str)
{
    if (!str) return strdup("");

    // Calculate needed size
    size_t needed = 0;
    for (const char *p = str; *p; p++) {
        switch (*p) {
            case '"':
            case '\\':
            case '\b':
            case '\f':
            case '\n':
            case '\r':
            case '\t':
                needed += 2;
                break;
            default:
                needed += 1;
                break;
        }
    }

    char *result = malloc(needed + 1);
    char *dst = result;

    for (const char *p = str; *p; p++) {
        switch (*p) {
            case '"':  *dst++ = '\\'; *dst++ = '"'; break;
            case '\\': *dst++ = '\\'; *dst++ = '\\'; break;
            case '\b': *dst++ = '\\'; *dst++ = 'b'; break;
            case '\f': *dst++ = '\\'; *dst++ = 'f'; break;
            case '\n': *dst++ = '\\'; *dst++ = 'n'; break;
            case '\r': *dst++ = '\\'; *dst++ = 'r'; break;
            case '\t': *dst++ = '\\'; *dst++ = 't'; break;
            default:   *dst++ = *p; break;
        }
    }
    *dst = '\0';

    return result;
}

static char *json_unescape(const char *str)
{
    if (!str) return NULL;

    char *result = malloc(strlen(str) + 1);
    char *dst = result;

    for (const char *p = str; *p; p++) {
        if (*p == '\\' && *(p + 1)) {
            p++;
            switch (*p) {
                case '"':  *dst++ = '"'; break;
                case '\\': *dst++ = '\\'; break;
                case 'b':  *dst++ = '\b'; break;
                case 'f':  *dst++ = '\f'; break;
                case 'n':  *dst++ = '\n'; break;
                case 'r':  *dst++ = '\r'; break;
                case 't':  *dst++ = '\t'; break;
                default:   *dst++ = *p; break;
            }
        } else {
            *dst++ = *p;
        }
    }
    *dst = '\0';

    return result;
}

static const char *redact_credential_header(const char *name, const char *value)
{
    // Case-insensitive header matching
    if (strcasecmp(name, "authorization") == 0) {
        if (strncasecmp(value, "Bearer ", 7) == 0) {
            return "Bearer REDACTED";
        }
        return "REDACTED";
    }

    if (strcasecmp(name, "x-api-key") == 0 ||
        strcasecmp(name, "x-goog-api-key") == 0 ||
        strcasecmp(name, "x-subscription-token") == 0) {
        return "REDACTED";
    }

    return value;
}

static void free_request_data(request_data_t *req)
{
    if (!req) return;
    free(req->method);
    free(req->url);
    free(req->headers);
    free(req->body);
}

static void free_chunk_queue(chunk_queue_t *queue)
{
    if (!queue) return;
    for (size_t i = 0; i < queue->count; i++) {
        free(queue->chunks[i]);
    }
    free(queue->chunks);
}
