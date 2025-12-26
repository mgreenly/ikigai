#include <check.h>
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>
#include "../../src/vendor/yyjson/yyjson.h"
#include "../../src/credentials.h"
#include "../../src/error.h"
#include "../test_utils.h"

/**
 * Anthropic Mock Verification Test Suite
 *
 * These tests verify that our test fixtures match the structure and format
 * of real Anthropic API responses. They only run when VERIFY_MOCKS=1 is set.
 *
 * Purpose:
 * - Ensure fixtures stay up-to-date with API changes
 * - Validate that our mocks accurately represent real API behavior
 * - Provide a way to capture/update fixtures when the API changes
 *
 * Usage:
 *   ANTHROPIC_API_KEY=sk-ant-... VERIFY_MOCKS=1 make check
 *   ANTHROPIC_API_KEY=sk-ant-... VERIFY_MOCKS=1 CAPTURE_FIXTURES=1 make check
 *
 * Note: These tests make real API calls and incur costs.
 */

/* Helper: Check if verification mode is enabled */
static bool should_verify_mocks(void)
{
    const char *verify = getenv("VERIFY_MOCKS");
    return verify != NULL && strcmp(verify, "1") == 0;
}

/* Helper: Check if fixture capture mode is enabled */
static bool should_capture_fixtures(void)
{
    const char *capture = getenv("CAPTURE_FIXTURES");
    return capture != NULL && strcmp(capture, "1") == 0;
}

/* Helper: Get API key from environment or credentials file */
static const char *get_api_key(TALLOC_CTX *ctx)
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

/* SSE event accumulator */
typedef struct {
    char **events;
    size_t count;
    size_t capacity;
} sse_accumulator_t;

/* Helper: Create SSE accumulator */
static sse_accumulator_t *create_sse_accumulator(TALLOC_CTX *ctx)
{
    sse_accumulator_t *acc = talloc_zero(ctx, sse_accumulator_t);
    ck_assert_ptr_nonnull(acc);

    acc->capacity = 32;
    acc->events = talloc_array_size(acc, sizeof(char *), (unsigned)acc->capacity);
    ck_assert_ptr_nonnull(acc->events);
    acc->count = 0;

    return acc;
}

/* Helper: Add event to accumulator */
static void add_sse_event(sse_accumulator_t *acc, const char *event)
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

/* SSE parser state */
typedef struct {
    sse_accumulator_t *acc;
    char *event_type;
    char *data_buffer;
    size_t data_len;
    size_t data_capacity;
} sse_parser_t;

/* Helper: Process empty line (end of event) */
static void process_empty_line(sse_parser_t *parser)
{
    if (parser->data_len > 0) {
        /* Null-terminate data */
        parser->data_buffer[parser->data_len] = '\0';

        /* Add complete event */
        add_sse_event(parser->acc, parser->data_buffer);

        /* Reset for next event */
        parser->data_len = 0;
        if (parser->event_type) {
            talloc_free(parser->event_type);
            parser->event_type = NULL;
        }
    }
}

/* Helper: Process data line */
static void process_data_line(sse_parser_t *parser, const char *line_start, const char *line_end)
{
    /* Skip "data:" prefix */
    const char *data_start = line_start + 5;
    while (data_start < line_end && (*data_start == ' ' || *data_start == '\t')) {
        data_start++;
    }
    size_t data_chunk_len = (size_t)(line_end - data_start);

    /* Ensure buffer has space */
    size_t needed = parser->data_len + data_chunk_len + 1;
    if (needed > parser->data_capacity) {
        parser->data_capacity = needed * 2;
        parser->data_buffer = talloc_realloc_size(parser, parser->data_buffer,
                                                  parser->data_capacity);
    }

    /* Append data */
    memcpy(parser->data_buffer + parser->data_len, data_start, data_chunk_len);
    parser->data_len += data_chunk_len;
}

/* Helper: Process a single SSE line */
static void process_sse_line(sse_parser_t *parser, const char *line_start, const char *line_end)
{
    size_t line_len = (size_t)(line_end - line_start);

    /* Skip \r if present */
    if (line_len > 0 && line_start[line_len - 1] == '\r') {
        line_len--;
        line_end--;
    }

    /* Empty line = end of event */
    if (line_len == 0) {
        process_empty_line(parser);
        return;
    }

    /* Parse event type */
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

    /* Parse data */
    if (line_len > 5 && strncmp(line_start, "data:", 5) == 0) {
        process_data_line(parser, line_start, line_end);
    }
}

/* Helper: Parse SSE stream */
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

/* Helper: Make HTTP POST request with SSE streaming */
static int http_post_sse(TALLOC_CTX *ctx, const char *url, const char *api_key,
                         const char *body, sse_accumulator_t *acc)
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

/* Helper: Make HTTP POST request (non-streaming) */
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

static int http_post_json(TALLOC_CTX *ctx, const char *url, const char *api_key,
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

/* Helper: Capture fixture to file */
static void capture_fixture(const char *name, sse_accumulator_t *acc)
{
    if (!should_capture_fixtures()) {
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

START_TEST(verify_anthropic_streaming_text) {
    /* Skip if not in verification mode */
    if (!should_verify_mocks()) {
        ck_assert(true);
        return;
    }

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    const char *api_key = get_api_key(ctx);
    if (!api_key) {
        ck_abort_msg("ANTHROPIC_API_KEY not set");
    }

    /* Build request */
    const char *request_body =
        "{"
        "\"model\":\"claude-sonnet-4-5-20250929\","
        "\"max_tokens\":100,"
        "\"stream\":true,"
        "\"messages\":[{\"role\":\"user\",\"content\":\"Say hello\"}]"
        "}";

    /* Make API call */
    sse_accumulator_t *acc = create_sse_accumulator(ctx);
    int status = http_post_sse(ctx, "https://api.anthropic.com/v1/messages",
                               api_key, request_body, acc);

    /* Verify HTTP status */
    ck_assert_int_eq(status, 200);
    ck_assert(acc->count > 0);

    /* Parse events and verify structure */
    bool seen_message_start = false;
    bool seen_content_block_start = false;
    bool seen_content_block_delta = false;
    bool seen_content_block_stop = false;
    bool seen_message_delta = false;
    bool seen_message_stop = false;

    for (size_t i = 0; i < acc->count; i++) {
        yyjson_doc *doc = yyjson_read(acc->events[i], strlen(acc->events[i]), 0);
        ck_assert_ptr_nonnull(doc);

        yyjson_val *root = yyjson_doc_get_root(doc);
        yyjson_val *type_val = yyjson_obj_get(root, "type");
        ck_assert_ptr_nonnull(type_val);

        const char *type = yyjson_get_str(type_val);
        ck_assert_ptr_nonnull(type);

        if (strcmp(type, "message_start") == 0) {
            seen_message_start = true;
            yyjson_val *message = yyjson_obj_get(root, "message");
            ck_assert_ptr_nonnull(message);
            ck_assert_ptr_nonnull(yyjson_obj_get(message, "id"));
            ck_assert_ptr_nonnull(yyjson_obj_get(message, "role"));
            ck_assert_ptr_nonnull(yyjson_obj_get(message, "model"));
        } else if (strcmp(type, "content_block_start") == 0) {
            seen_content_block_start = true;
            ck_assert_ptr_nonnull(yyjson_obj_get(root, "index"));
            yyjson_val *block = yyjson_obj_get(root, "content_block");
            ck_assert_ptr_nonnull(block);
            yyjson_val *block_type = yyjson_obj_get(block, "type");
            ck_assert_str_eq(yyjson_get_str(block_type), "text");
        } else if (strcmp(type, "content_block_delta") == 0) {
            seen_content_block_delta = true;
            yyjson_val *delta = yyjson_obj_get(root, "delta");
            ck_assert_ptr_nonnull(delta);
            yyjson_val *delta_type = yyjson_obj_get(delta, "type");
            ck_assert_str_eq(yyjson_get_str(delta_type), "text_delta");
            ck_assert_ptr_nonnull(yyjson_obj_get(delta, "text"));
        } else if (strcmp(type, "content_block_stop") == 0) {
            seen_content_block_stop = true;
        } else if (strcmp(type, "message_delta") == 0) {
            seen_message_delta = true;
            yyjson_val *delta = yyjson_obj_get(root, "delta");
            ck_assert_ptr_nonnull(delta);
            ck_assert_ptr_nonnull(yyjson_obj_get(delta, "stop_reason"));
            ck_assert_ptr_nonnull(yyjson_obj_get(root, "usage"));
        } else if (strcmp(type, "message_stop") == 0) {
            seen_message_stop = true;
        }

        yyjson_doc_free(doc);
    }

    /* Verify event order */
    ck_assert(seen_message_start);
    ck_assert(seen_content_block_start);
    ck_assert(seen_content_block_delta);
    ck_assert(seen_content_block_stop);
    ck_assert(seen_message_delta);
    ck_assert(seen_message_stop);

    /* Optionally capture fixture */
    capture_fixture("stream_text_basic", acc);

    talloc_free(ctx);
}
END_TEST START_TEST(verify_anthropic_streaming_thinking)
{
    /* Skip if not in verification mode */
    if (!should_verify_mocks()) {
        ck_assert(true);
        return;
    }

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    const char *api_key = get_api_key(ctx);
    if (!api_key) {
        ck_abort_msg("ANTHROPIC_API_KEY not set");
    }

    /* Build request with thinking enabled */
    const char *request_body =
        "{"
        "\"model\":\"claude-sonnet-4-5-20250929\","
        "\"max_tokens\":1000,"
        "\"stream\":true,"
        "\"thinking\":{\"type\":\"enabled\",\"budget_tokens\":500},"
        "\"messages\":[{\"role\":\"user\",\"content\":\"What is 15 * 17?\"}]"
        "}";

    /* Make API call */
    sse_accumulator_t *acc = create_sse_accumulator(ctx);
    int status = http_post_sse(ctx, "https://api.anthropic.com/v1/messages",
                               api_key, request_body, acc);

    /* Verify HTTP status */
    ck_assert_int_eq(status, 200);
    ck_assert(acc->count > 0);

    /* Parse events and verify thinking block */
    bool seen_thinking_start = false;
    bool seen_thinking_delta = false;
    bool seen_text_start = false;
    int thinking_index = -1;
    int text_index = -1;

    for (size_t i = 0; i < acc->count; i++) {
        yyjson_doc *doc = yyjson_read(acc->events[i], strlen(acc->events[i]), 0);
        ck_assert_ptr_nonnull(doc);

        yyjson_val *root = yyjson_doc_get_root(doc);
        yyjson_val *type_val = yyjson_obj_get(root, "type");
        const char *type = yyjson_get_str(type_val);

        if (strcmp(type, "content_block_start") == 0) {
            yyjson_val *index = yyjson_obj_get(root, "index");
            int idx = (int)yyjson_get_int(index);
            yyjson_val *block = yyjson_obj_get(root, "content_block");
            yyjson_val *block_type = yyjson_obj_get(block, "type");
            const char *bt = yyjson_get_str(block_type);

            if (strcmp(bt, "thinking") == 0) {
                seen_thinking_start = true;
                thinking_index = idx;
            } else if (strcmp(bt, "text") == 0) {
                seen_text_start = true;
                text_index = idx;
            }
        } else if (strcmp(type, "content_block_delta") == 0) {
            yyjson_val *delta = yyjson_obj_get(root, "delta");
            yyjson_val *delta_type = yyjson_obj_get(delta, "type");
            const char *dt = yyjson_get_str(delta_type);

            if (strcmp(dt, "thinking_delta") == 0) {
                seen_thinking_delta = true;
            }
        }

        yyjson_doc_free(doc);
    }

    /* Verify thinking structure */
    ck_assert(seen_thinking_start);
    ck_assert(seen_thinking_delta);
    ck_assert(seen_text_start);
    ck_assert_int_eq(thinking_index, 0);
    ck_assert_int_eq(text_index, 1);

    /* Optionally capture fixture */
    capture_fixture("stream_text_thinking", acc);

    talloc_free(ctx);
}

END_TEST START_TEST(verify_anthropic_tool_call)
{
    /* Skip if not in verification mode */
    if (!should_verify_mocks()) {
        ck_assert(true);
        return;
    }

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    const char *api_key = get_api_key(ctx);
    if (!api_key) {
        ck_abort_msg("ANTHROPIC_API_KEY not set");
    }

    /* Build request with tool definition */
    const char *request_body =
        "{"
        "\"model\":\"claude-sonnet-4-5-20250929\","
        "\"max_tokens\":500,"
        "\"stream\":true,"
        "\"tools\":[{"
        "\"name\":\"get_weather\","
        "\"description\":\"Get weather for a location\","
        "\"input_schema\":{"
        "\"type\":\"object\","
        "\"properties\":{\"location\":{\"type\":\"string\"}},"
        "\"required\":[\"location\"]"
        "}"
        "}],"
        "\"messages\":[{\"role\":\"user\",\"content\":\"What's the weather in Paris?\"}]"
        "}";

    /* Make API call */
    sse_accumulator_t *acc = create_sse_accumulator(ctx);
    int status = http_post_sse(ctx, "https://api.anthropic.com/v1/messages",
                               api_key, request_body, acc);

    /* Verify HTTP status */
    ck_assert_int_eq(status, 200);
    ck_assert(acc->count > 0);

    /* Parse events and verify tool use */
    bool seen_tool_use = false;
    bool seen_input_json_delta = false;
    const char *tool_id = NULL;
    const char *stop_reason = NULL;

    for (size_t i = 0; i < acc->count; i++) {
        yyjson_doc *doc = yyjson_read(acc->events[i], strlen(acc->events[i]), 0);
        yyjson_val *root = yyjson_doc_get_root(doc);
        yyjson_val *type_val = yyjson_obj_get(root, "type");
        const char *type = yyjson_get_str(type_val);

        if (strcmp(type, "content_block_start") == 0) {
            yyjson_val *block = yyjson_obj_get(root, "content_block");
            yyjson_val *block_type = yyjson_obj_get(block, "type");
            const char *bt = yyjson_get_str(block_type);

            if (strcmp(bt, "tool_use") == 0) {
                seen_tool_use = true;
                yyjson_val *id = yyjson_obj_get(block, "id");
                const char *id_str = yyjson_get_str(id);
                ck_assert(strncmp(id_str, "toolu_", 6) == 0);
                tool_id = id_str;
                ck_assert_ptr_nonnull(yyjson_obj_get(block, "name"));
            }
        } else if (strcmp(type, "content_block_delta") == 0) {
            yyjson_val *delta = yyjson_obj_get(root, "delta");
            yyjson_val *delta_type = yyjson_obj_get(delta, "type");
            const char *dt = yyjson_get_str(delta_type);

            if (strcmp(dt, "input_json_delta") == 0) {
                seen_input_json_delta = true;
            }
        } else if (strcmp(type, "message_delta") == 0) {
            yyjson_val *delta = yyjson_obj_get(root, "delta");
            yyjson_val *sr = yyjson_obj_get(delta, "stop_reason");
            if (sr) {
                stop_reason = yyjson_get_str(sr);
            }
        }

        yyjson_doc_free(doc);
    }

    /* Verify tool call structure */
    ck_assert(seen_tool_use);
    ck_assert(seen_input_json_delta);
    ck_assert_ptr_nonnull(tool_id);
    if (stop_reason) {
        ck_assert_str_eq(stop_reason, "tool_use");
    }

    /* Optionally capture fixture */
    capture_fixture("stream_tool_call", acc);

    talloc_free(ctx);
}

END_TEST START_TEST(verify_anthropic_error_auth)
{
    /* Skip if not in verification mode */
    if (!should_verify_mocks()) {
        ck_assert(true);
        return;
    }

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    /* Use invalid API key */
    const char *invalid_key = "sk-ant-invalid";

    const char *request_body =
        "{"
        "\"model\":\"claude-sonnet-4-5-20250929\","
        "\"max_tokens\":100,"
        "\"messages\":[{\"role\":\"user\",\"content\":\"Hello\"}]"
        "}";

    /* Make API call (should fail) */
    char *response = NULL;
    int status = http_post_json(ctx, "https://api.anthropic.com/v1/messages",
                                invalid_key, request_body, &response);

    /* Verify HTTP status 401 */
    ck_assert_int_eq(status, 401);
    ck_assert_ptr_nonnull(response);

    /* Parse error response */
    yyjson_doc *doc = yyjson_read(response, strlen(response), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *type_val = yyjson_obj_get(root, "type");
    ck_assert_str_eq(yyjson_get_str(type_val), "error");

    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);

    yyjson_val *error_type = yyjson_obj_get(error, "type");
    ck_assert_str_eq(yyjson_get_str(error_type), "authentication_error");

    ck_assert_ptr_nonnull(yyjson_obj_get(error, "message"));

    yyjson_doc_free(doc);

    /* Optionally capture fixture */
    if (should_capture_fixtures()) {
        char path[512];
        snprintf(path, sizeof(path), "tests/fixtures/vcr/anthropic/error_401_auth.json");
        FILE *f = fopen(path, "w");
        if (f) {
            fprintf(f, "%s\n", response);
            fclose(f);
            fprintf(stderr, "Captured fixture: %s\n", path);
        }
    }

    talloc_free(ctx);
}

END_TEST START_TEST(validate_fixture_structure)
{
    /* This test runs even without VERIFY_MOCKS to validate fixture files */
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    /* Check if fixtures exist - if not, skip test */
    struct stat st;
    if (stat("tests/fixtures/vcr/anthropic/stream_text_basic.jsonl", &st) != 0) {
        /* Fixtures not yet created - skip */
        talloc_free(ctx);
        return;
    }

    /* Validate each fixture has correct JSON structure */
    const char *fixtures[] = {
        "tests/fixtures/vcr/anthropic/stream_text_basic.jsonl",
        "tests/fixtures/vcr/anthropic/stream_text_thinking.jsonl",
        "tests/fixtures/vcr/anthropic/stream_tool_call.jsonl",
        "tests/fixtures/vcr/anthropic/error_401_auth.json",
        NULL
    };

    for (int i = 0; fixtures[i] != NULL; i++) {
        if (stat(fixtures[i], &st) != 0) {
            continue; /* Fixture doesn't exist yet */
        }

        char *content = load_file_to_string(ctx, fixtures[i]);
        ck_assert_ptr_nonnull(content);

        /* For JSONL files, validate each line */
        if (strstr(fixtures[i], ".jsonl")) {
            char *line = strtok(content, "\n");
            while (line != NULL) {
                if (strlen(line) > 0) {
                    yyjson_doc *doc = yyjson_read(line, strlen(line), 0);
                    ck_assert_ptr_nonnull(doc);
                    yyjson_doc_free(doc);
                }
                line = strtok(NULL, "\n");
            }
        } else {
            /* For JSON files, validate single object */
            yyjson_doc *doc = yyjson_read(content, strlen(content), 0);
            ck_assert_ptr_nonnull(doc);
            yyjson_doc_free(doc);
        }
    }

    talloc_free(ctx);
}

END_TEST

static Suite *anthropic_mock_verification_suite(void)
{
    Suite *s = suite_create("AnthropicMockVerification");
    TCase *tc_core = tcase_create("Core");

    /* Set longer timeout for real API calls */
    tcase_set_timeout(tc_core, 60);

    /* Verification tests (only run with VERIFY_MOCKS=1) */
    tcase_add_test(tc_core, verify_anthropic_streaming_text);
    tcase_add_test(tc_core, verify_anthropic_streaming_thinking);
    tcase_add_test(tc_core, verify_anthropic_tool_call);
    tcase_add_test(tc_core, verify_anthropic_error_auth);

    /* Fixture validation (always runs) */
    tcase_add_test(tc_core, validate_fixture_structure);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = anthropic_mock_verification_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
