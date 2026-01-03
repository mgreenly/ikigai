#include "anthropic_mock_verification_helpers.h"

#include <check.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>
#include "../../src/credentials.h"
#include "../../src/error.h"

// Helper: Check if verification mode is enabled
bool should_verify_mocks_anthropic(void)
{
    const char *verify = getenv("VERIFY_MOCKS");
    return verify != NULL && strcmp(verify, "1") == 0;
}

// Helper: Check if fixture capture mode is enabled
bool should_capture_fixtures_anthropic(void)
{
    const char *capture = getenv("CAPTURE_FIXTURES");
    return capture != NULL && strcmp(capture, "1") == 0;
}

// Helper: Get API key from environment or credentials file
const char *get_api_key_anthropic(TALLOC_CTX *ctx)
{
    const char *env_key = getenv("ANTHROPIC_API_KEY");
    if (env_key) {
        return env_key;
    }

    ik_credentials_t *creds;
    res_t res = ik_credentials_load(ctx, NULL, &creds);
    if (res.is_err) {
        return NULL;
    }

    return ik_credentials_get(creds, "anthropic");
}

// Helper: Create SSE accumulator
sse_event_accumulator_t *create_sse_event_accumulator(TALLOC_CTX *ctx)
{
    sse_event_accumulator_t *acc = talloc_zero(ctx, sse_event_accumulator_t);
    ck_assert_ptr_nonnull(acc);

    acc->capacity = 32;
    acc->events = talloc_array_size(acc, sizeof(char *), (unsigned)acc->capacity);
    ck_assert_ptr_nonnull(acc->events);
    acc->count = 0;

    return acc;
}

// Helper: Add event to accumulator
void add_sse_event(sse_event_accumulator_t *acc, const char *event)
{
    if (acc->count >= acc->capacity) {
        acc->capacity *= 2;
        acc->events = talloc_realloc_size(acc, acc->events, sizeof(char *) * acc->capacity);
        ck_assert_ptr_nonnull(acc->events);
    }

    acc->events[acc->count] = talloc_strdup(acc->events, event);
    ck_assert_ptr_nonnull(acc->events[acc->count]);
    acc->count++;
}

// SSE parser state
typedef struct {
    sse_event_accumulator_t *acc;
    char *event_type;
    char *data_buffer;
    size_t data_len;
    size_t data_capacity;
} sse_parser_t;

// Helper: Process empty line (end of event)
static void process_empty_line(sse_parser_t *parser)
{
    if (parser->data_len > 0) {
        // Null-terminate data
        parser->data_buffer[parser->data_len] = '\0';

        // Add complete event
        add_sse_event(parser->acc, parser->data_buffer);

        // Reset for next event
        parser->data_len = 0;
        if (parser->event_type) {
            talloc_free(parser->event_type);
            parser->event_type = NULL;
        }
    }
}

// Helper: Process data line
static void process_data_line(sse_parser_t *parser, const char *line_start, const char *line_end)
{
    // Skip "data:" prefix
    const char *data_start = line_start + 5;
    while (data_start < line_end && (*data_start == ' ' || *data_start == '\t')) {
        data_start++;
    }
    size_t data_chunk_len = (size_t)(line_end - data_start);

    // Ensure buffer has space
    size_t needed = parser->data_len + data_chunk_len + 1;
    if (needed > parser->data_capacity) {
        parser->data_capacity = needed * 2;
        parser->data_buffer = talloc_realloc_size(parser, parser->data_buffer,
                                                  parser->data_capacity);
    }

    // Append data
    memcpy(parser->data_buffer + parser->data_len, data_start, data_chunk_len);
    parser->data_len += data_chunk_len;
}

// Helper: Process a single SSE line
static void process_sse_line(sse_parser_t *parser, const char *line_start, const char *line_end)
{
    size_t line_len = (size_t)(line_end - line_start);

    // Skip \r if present
    if (line_len > 0 && line_start[line_len - 1] == '\r') {
        line_len--;
        line_end--;
    }

    // Empty line = end of event
    if (line_len == 0) {
        process_empty_line(parser);
        return;
    }

    // Parse event type
    if (line_len > 6 && strncmp(line_start, "event:", 6) == 0) {
        const char *event_start = line_start + 6;
        while (event_start < line_end && (*event_start == ' ' || *event_start == '\t')) {
            event_start++;
        }
        size_t event_len = (size_t)(line_end - event_start);
        if (parser->event_type) {
            talloc_free(parser->event_type);
        }
        parser->event_type = talloc_strndup(parser, event_start, event_len);
        return;
    }

    // Parse data
    if (line_len > 5 && strncmp(line_start, "data:", 5) == 0) {
        process_data_line(parser, line_start, line_end);
    }
}

// Helper: Parse SSE stream
static size_t sse_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    sse_parser_t *parser = (sse_parser_t *)userdata;
    size_t total = size * nmemb;
    char *line_start = ptr;

    for (size_t i = 0; i < total; i++) {
        if (ptr[i] == '\n') {
            char *line_end = &ptr[i];
            process_sse_line(parser, line_start, line_end);
            line_start = &ptr[i + 1];
        }
    }

    return total;
}

// Helper: Make HTTP POST request with SSE streaming
int http_post_sse_anthropic(TALLOC_CTX *ctx, const char *url, const char *api_key,
                            const char *body, sse_event_accumulator_t *acc)
{
    CURL *curl = curl_easy_init();
    ck_assert_ptr_nonnull(curl);

    struct curl_slist *headers = NULL;
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "x-api-key: %s", api_key);

    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");

    sse_parser_t parser = {
        .acc = acc,
        .event_type = NULL,
        .data_buffer = talloc_size(ctx, 4096),
        .data_len = 0,
        .data_capacity = 4096
    };

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, sse_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &parser);

    CURLcode res = curl_easy_perform(curl);
    ck_assert_int_eq(res, CURLE_OK);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    talloc_free(parser.data_buffer);
    if (parser.event_type) {
        talloc_free(parser.event_type);
    }

    return (int)http_code;
}

// Helper: Make HTTP POST request (non-streaming)
typedef struct {
    char *buffer;
    size_t size;
} response_buffer_t;

static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    response_buffer_t *buf = (response_buffer_t *)userdata;
    size_t total = size * nmemb;

    char *new_buffer = talloc_realloc_size(NULL, buf->buffer, buf->size + total + 1);
    if (!new_buffer) {
        return 0;
    }

    memcpy(new_buffer + buf->size, ptr, total);
    buf->size += total;
    new_buffer[buf->size] = '\0';
    buf->buffer = new_buffer;

    return total;
}

int http_post_json_anthropic(TALLOC_CTX *ctx, const char *url, const char *api_key,
                             const char *body, char **out_response)
{
    CURL *curl = curl_easy_init();
    ck_assert_ptr_nonnull(curl);

    struct curl_slist *headers = NULL;
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "x-api-key: %s", api_key);

    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");

    response_buffer_t resp = {
        .buffer = talloc_size(NULL, 1),
        .size = 0
    };
    resp.buffer[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);

    CURLcode res = curl_easy_perform(curl);
    ck_assert_int_eq(res, CURLE_OK);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    *out_response = talloc_steal(ctx, resp.buffer);

    return (int)http_code;
}

// Helper: Capture fixture to file
void capture_fixture_anthropic(const char *name, sse_event_accumulator_t *acc)
{
    if (!should_capture_fixtures_anthropic()) {
        return;
    }

    char path[512];
    snprintf(path, sizeof(path), "tests/fixtures/vcr/anthropic/%s.jsonl", name);

    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "Warning: Failed to open %s for writing\n", path);
        return;
    }

    for (size_t i = 0; i < acc->count; i++) {
        fprintf(f, "%s\n", acc->events[i]);
    }

    fclose(f);
    fprintf(stderr, "Captured fixture: %s\n", path);
}
