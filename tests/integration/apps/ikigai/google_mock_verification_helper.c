#include "google_mock_verification_helper.h"

#include <check.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>
#include "shared/credentials.h"
#include "shared/error.h"

// Helper: Check if verification mode is enabled
bool should_verify_mocks(void)
{
    const char *verify = getenv("VERIFY_MOCKS");
    return verify != NULL && strcmp(verify, "1") == 0;
}

// Helper: Check if fixture capture mode is enabled
bool should_capture_fixtures(void)
{
    const char *capture = getenv("CAPTURE_FIXTURES");
    return capture != NULL && strcmp(capture, "1") == 0;
}

// Helper: Get API key from environment or credentials file
const char *get_api_key_google(TALLOC_CTX *ctx)
{
    const char *env_key = getenv("GOOGLE_API_KEY");
    if (env_key) {
        return env_key;
    }

    ik_credentials_t *creds;
    res_t res = ik_credentials_load(ctx, NULL, &creds);
    if (res.is_err) {
        return NULL;
    }

    return ik_credentials_get(creds, "GOOGLE_API_KEY");
}

// Helper: Create SSE accumulator
sse_accumulator_t *create_sse_accumulator(TALLOC_CTX *ctx)
{
    sse_accumulator_t *acc = talloc_zero(ctx, sse_accumulator_t);
    ck_assert_ptr_nonnull(acc);

    acc->capacity = 32;
    acc->chunks = talloc_array_size(acc, sizeof(char *), (unsigned)acc->capacity);
    ck_assert_ptr_nonnull(acc->chunks);
    acc->count = 0;

    return acc;
}

// Helper: Add chunk to accumulator
void add_sse_chunk(sse_accumulator_t *acc, const char *chunk)
{
    if (acc->count >= acc->capacity) {
        acc->capacity *= 2;
        acc->chunks = talloc_realloc_size(acc, acc->chunks, sizeof(char *) * acc->capacity);
        ck_assert_ptr_nonnull(acc->chunks);
    }

    acc->chunks[acc->count] = talloc_strdup(acc->chunks, chunk);
    ck_assert_ptr_nonnull(acc->chunks[acc->count]);
    acc->count++;
}

// SSE parser state for Google
typedef struct {
    sse_accumulator_t *acc;
    char *data_buffer;
    size_t data_len;
    size_t data_capacity;
} sse_parser_t;

// Helper: Parse Google SSE stream (data: JSON format)
static size_t sse_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    sse_parser_t *parser = (sse_parser_t *)userdata;
    size_t total = size * nmemb;
    char *line_start = ptr;
    char *line_end;

    for (size_t i = 0; i < total; i++) {
        if (ptr[i] == '\n') {
            line_end = &ptr[i];
            size_t line_len = (size_t)(line_end - line_start);

            // Skip \r if present
            if (line_len > 0 && line_start[line_len - 1] == '\r') {
                line_len--;
            }

            // Parse "data: " prefix
            if (line_len > 6 && strncmp(line_start, "data: ", 6) == 0) {
                const char *json_start = line_start + 6;
                size_t json_len = line_len - 6;

                // Ensure buffer has space
                size_t needed = json_len + 1;
                if (needed > parser->data_capacity) {
                    parser->data_capacity = needed * 2;
                    parser->data_buffer = talloc_realloc_size(parser, parser->data_buffer,
                                                              parser->data_capacity);
                }

                // Copy JSON
                memcpy(parser->data_buffer, json_start, json_len);
                parser->data_buffer[json_len] = '\0';

                // Add chunk
                add_sse_chunk(parser->acc, parser->data_buffer);
            }

            line_start = &ptr[i + 1];
        }
    }

    return total;
}

// Helper: Make HTTP POST request with SSE streaming
int http_post_sse_google(TALLOC_CTX *ctx, const char *url,
                         const char *body, sse_accumulator_t *acc)
{
    CURL *curl = curl_easy_init();
    ck_assert_ptr_nonnull(curl);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    sse_parser_t parser = {
        .acc = acc,
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

int http_post_json_google(TALLOC_CTX *ctx, const char *url,
                          const char *body, char **out_response)
{
    CURL *curl = curl_easy_init();
    ck_assert_ptr_nonnull(curl);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

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
void capture_fixture_google(const char *name, sse_accumulator_t *acc)
{
    if (!should_capture_fixtures()) {
        return;
    }

    char path[512];
    snprintf(path, sizeof(path), "tests/fixtures/vcr/google/%s.jsonl", name);

    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "Warning: Failed to open %s for writing\n", path);
        return;
    }

    for (size_t i = 0; i < acc->count; i++) {
        fprintf(f, "%s\n", acc->chunks[i]);
    }

    fclose(f);
    fprintf(stderr, "Captured fixture: %s\n", path);
}
