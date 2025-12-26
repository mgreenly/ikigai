#include "google_mock_verification_helpers.h"

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>
#include "../../src/vendor/yyjson/yyjson.h"
#include "../test_utils.h"

/**
 * Google Gemini Mock Verification Test Suite
 *
 * These tests verify that our test fixtures match the structure and format
 * of real Google Gemini API responses. They only run when VERIFY_MOCKS=1 is set.
 *
 * Purpose:
 * - Ensure fixtures stay up-to-date with API changes
 * - Validate that our mocks accurately represent real API behavior
 * - Provide a way to capture/update fixtures when the API changes
 *
 * Usage:
 *   GOOGLE_API_KEY=... VERIFY_MOCKS=1 make check
 *   GOOGLE_API_KEY=... VERIFY_MOCKS=1 CAPTURE_FIXTURES=1 make check
 *
 * Note: These tests make real API calls and incur costs.
 */

// Helper: Process a single streaming chunk for text verification
static void process_text_chunk(yyjson_val *root, bool *seen_text, bool *seen_finish_reason, bool *seen_usage)
{
    yyjson_val *candidates = yyjson_obj_get(root, "candidates");
    ck_assert_ptr_nonnull(candidates);
    ck_assert(yyjson_is_arr(candidates));

    yyjson_val *candidate = yyjson_arr_get_first(candidates);
    ck_assert_ptr_nonnull(candidate);

    // Check for content
    yyjson_val *content = yyjson_obj_get(candidate, "content");
    if (content) {
        yyjson_val *parts = yyjson_obj_get(content, "parts");
        if (parts && yyjson_is_arr(parts)) {
            yyjson_val *part = yyjson_arr_get_first(parts);
            if (part) {
                yyjson_val *text = yyjson_obj_get(part, "text");
                if (text) {
                    *seen_text = true;
                }
            }
        }
    }

    // Check for finish reason
    yyjson_val *finish_reason = yyjson_obj_get(candidate, "finishReason");
    if (finish_reason) {
        *seen_finish_reason = true;
        const char *reason = yyjson_get_str(finish_reason);
        ck_assert_ptr_nonnull(reason);
    }

    // Check for usage metadata
    yyjson_val *usage = yyjson_obj_get(root, "usageMetadata");
    if (usage) {
        *seen_usage = true;
        ck_assert_ptr_nonnull(yyjson_obj_get(usage, "promptTokenCount"));
        ck_assert_ptr_nonnull(yyjson_obj_get(usage, "candidatesTokenCount"));
        ck_assert_ptr_nonnull(yyjson_obj_get(usage, "totalTokenCount"));
    }
}

START_TEST(verify_google_streaming_text) {
    // Skip if not in verification mode
    if (!should_verify_mocks()) {
        ck_assert(true);
        return;
    }

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    const char *api_key = get_api_key_google(ctx);
    if (!api_key) {
        ck_abort_msg("GOOGLE_API_KEY not set");
    }

    // Build request URL with API key
    char url[512];
    snprintf(url,
             sizeof(url),
             "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:streamGenerateContent?alt=sse&key=%s",
             api_key);

    // Build request body
    const char *request_body =
        "{"
        "\"contents\":[{\"role\":\"user\",\"parts\":[{\"text\":\"Say hello\"}]}],"
        "\"generationConfig\":{\"maxOutputTokens\":100}"
        "}";

    // Make API call
    sse_accumulator_t *acc = create_sse_accumulator(ctx);
    int status = http_post_sse_google(ctx, url, request_body, acc);

    // Verify HTTP status
    ck_assert_int_eq(status, 200);
    ck_assert(acc->count > 0);

    // Parse chunks and verify structure
    bool seen_text = false;
    bool seen_finish_reason = false;
    bool seen_usage = false;

    for (size_t i = 0; i < acc->count; i++) {
        yyjson_doc *doc = yyjson_read(acc->chunks[i], strlen(acc->chunks[i]), 0);
        ck_assert_ptr_nonnull(doc);

        yyjson_val *root = yyjson_doc_get_root(doc);
        process_text_chunk(root, &seen_text, &seen_finish_reason, &seen_usage);

        yyjson_doc_free(doc);
    }

    // Verify required fields were present
    ck_assert(seen_text);
    ck_assert(seen_finish_reason);
    ck_assert(seen_usage);

    // Optionally capture fixture
    capture_fixture_google("stream_text_basic", acc);

    talloc_free(ctx);
}

// Helper: Process a single streaming chunk for thinking verification
static void process_thinking_chunk(yyjson_val *root, bool *seen_thinking, bool *seen_regular_text)
{
    yyjson_val *candidates = yyjson_obj_get(root, "candidates");
    if (!candidates) {
        return;
    }

    yyjson_val *candidate = yyjson_arr_get_first(candidates);
    if (!candidate) {
        return;
    }

    yyjson_val *content = yyjson_obj_get(candidate, "content");
    if (!content) {
        return;
    }

    yyjson_val *parts = yyjson_obj_get(content, "parts");
    if (!parts || !yyjson_is_arr(parts)) {
        return;
    }

    size_t idx, max;
    yyjson_val *part;
    yyjson_arr_foreach(parts, idx, max, part) {
        yyjson_val *text = yyjson_obj_get(part, "text");
        yyjson_val *thought = yyjson_obj_get(part, "thought");

        if (text) {
            if (thought && yyjson_get_bool(thought)) {
                *seen_thinking = true;
            } else {
                *seen_regular_text = true;
            }
        }
    }
}

END_TEST START_TEST(verify_google_streaming_thinking)
{
    // Skip if not in verification mode
    if (!should_verify_mocks()) {
        ck_assert(true);
        return;
    }

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    const char *api_key = get_api_key_google(ctx);
    if (!api_key) {
        ck_abort_msg("GOOGLE_API_KEY not set");
    }

    // Build request URL with API key
    char url[512];
    snprintf(url,
             sizeof(url),
             "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:streamGenerateContent?alt=sse&key=%s",
             api_key);

    // Build request body with thinking config
    const char *request_body =
        "{"
        "\"contents\":[{\"role\":\"user\",\"parts\":[{\"text\":\"What is 15 * 17?\"}]}],"
        "\"generationConfig\":{"
        "\"maxOutputTokens\":1000,"
        "\"thinkingConfig\":{\"thinkingBudget\":500}"
        "}"
        "}";

    // Make API call
    sse_accumulator_t *acc = create_sse_accumulator(ctx);
    int status = http_post_sse_google(ctx, url, request_body, acc);

    // Verify HTTP status
    ck_assert_int_eq(status, 200);
    ck_assert(acc->count > 0);

    // Parse chunks and look for thinking parts
    bool seen_thinking = false;
    bool seen_regular_text = false;

    for (size_t i = 0; i < acc->count; i++) {
        yyjson_doc *doc = yyjson_read(acc->chunks[i], strlen(acc->chunks[i]), 0);
        ck_assert_ptr_nonnull(doc);

        yyjson_val *root = yyjson_doc_get_root(doc);
        process_thinking_chunk(root, &seen_thinking, &seen_regular_text);

        yyjson_doc_free(doc);
    }

    // Note: Thinking may not always be present depending on model version
    // Just verify we got text output
    ck_assert(seen_regular_text || seen_thinking);

    // Optionally capture fixture
    capture_fixture_google("stream_text_thinking", acc);

    talloc_free(ctx);
}

// Helper: Process a single streaming chunk for tool call verification
static void process_tool_call_chunk(yyjson_val *root, bool *seen_function_call, const char **function_name)
{
    yyjson_val *candidates = yyjson_obj_get(root, "candidates");
    if (!candidates) {
        return;
    }

    yyjson_val *candidate = yyjson_arr_get_first(candidates);
    if (!candidate) {
        return;
    }

    yyjson_val *content = yyjson_obj_get(candidate, "content");
    if (!content) {
        return;
    }

    yyjson_val *parts = yyjson_obj_get(content, "parts");
    if (!parts || !yyjson_is_arr(parts)) {
        return;
    }

    size_t idx, max;
    yyjson_val *part;
    yyjson_arr_foreach(parts, idx, max, part) {
        yyjson_val *function_call = yyjson_obj_get(part, "functionCall");
        if (function_call) {
            *seen_function_call = true;
            yyjson_val *name = yyjson_obj_get(function_call, "name");
            *function_name = yyjson_get_str(name);
            yyjson_val *args = yyjson_obj_get(function_call, "args");
            ck_assert_ptr_nonnull(args);
        }
    }
}

END_TEST START_TEST(verify_google_tool_call)
{
    // Skip if not in verification mode
    if (!should_verify_mocks()) {
        ck_assert(true);
        return;
    }

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    const char *api_key = get_api_key_google(ctx);
    if (!api_key) {
        ck_abort_msg("GOOGLE_API_KEY not set");
    }

    // Build request URL with API key
    char url[512];
    snprintf(url,
             sizeof(url),
             "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:streamGenerateContent?alt=sse&key=%s",
             api_key);

    // Build request body with function declaration
    const char *request_body =
        "{"
        "\"contents\":[{\"role\":\"user\",\"parts\":[{\"text\":\"What's the weather in Paris?\"}]}],"
        "\"tools\":[{"
        "\"functionDeclarations\":[{"
        "\"name\":\"get_weather\","
        "\"description\":\"Get weather for a location\","
        "\"parameters\":{"
        "\"type\":\"object\","
        "\"properties\":{\"location\":{\"type\":\"string\"}},"
        "\"required\":[\"location\"]"
        "}"
        "}]"
        "}]"
        "}";

    // Make API call
    sse_accumulator_t *acc = create_sse_accumulator(ctx);
    int status = http_post_sse_google(ctx, url, request_body, acc);

    // Verify HTTP status
    ck_assert_int_eq(status, 200);
    ck_assert(acc->count > 0);

    // Parse chunks and look for function call
    bool seen_function_call = false;
    const char *function_name = NULL;

    for (size_t i = 0; i < acc->count; i++) {
        yyjson_doc *doc = yyjson_read(acc->chunks[i], strlen(acc->chunks[i]), 0);
        ck_assert_ptr_nonnull(doc);

        yyjson_val *root = yyjson_doc_get_root(doc);
        process_tool_call_chunk(root, &seen_function_call, &function_name);

        yyjson_doc_free(doc);
    }

    // Verify function call structure
    ck_assert(seen_function_call);
    ck_assert_ptr_nonnull(function_name);

    // Optionally capture fixture
    capture_fixture_google("stream_tool_call", acc);

    talloc_free(ctx);
}

END_TEST START_TEST(verify_google_error_auth)
{
    // Skip if not in verification mode
    if (!should_verify_mocks()) {
        ck_assert(true);
        return;
    }

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Use invalid API key
    const char *invalid_key = "invalid_key";

    // Build request URL with invalid API key
    char url[512];
    snprintf(url, sizeof(url),
             "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent?key=%s",
             invalid_key);

    const char *request_body =
        "{"
        "\"contents\":[{\"role\":\"user\",\"parts\":[{\"text\":\"Hello\"}]}]"
        "}";

    // Make API call (should fail)
    char *response = NULL;
    int status = http_post_json_google(ctx, url, request_body, &response);

    // Verify HTTP status 400 or 401 or 403
    ck_assert(status >= 400 && status < 500);
    ck_assert_ptr_nonnull(response);

    // Parse error response
    yyjson_doc *doc = yyjson_read(response, strlen(response), 0);
    ck_assert_ptr_nonnull(doc);

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_ptr_nonnull(error);

    ck_assert_ptr_nonnull(yyjson_obj_get(error, "code"));
    ck_assert_ptr_nonnull(yyjson_obj_get(error, "message"));
    ck_assert_ptr_nonnull(yyjson_obj_get(error, "status"));

    yyjson_doc_free(doc);

    // Optionally capture fixture
    if (should_capture_fixtures()) {
        char path[512];
        snprintf(path, sizeof(path), "tests/fixtures/vcr/google/error_401_auth.json");
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
    // This test runs even without VERIFY_MOCKS to validate fixture files
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Check if fixtures exist - if not, skip test
    struct stat st;
    if (stat("tests/fixtures/vcr/google/stream_text_basic.jsonl", &st) != 0) {
        // Fixtures not yet created - skip
        talloc_free(ctx);
        return;
    }

    // Validate each fixture has correct JSON structure
    const char *fixtures[] = {
        "tests/fixtures/vcr/google/stream_text_basic.jsonl",
        "tests/fixtures/vcr/google/stream_text_thinking.jsonl",
        "tests/fixtures/vcr/google/stream_tool_call.jsonl",
        "tests/fixtures/vcr/google/error_401_auth.json",
        NULL
    };

    for (int i = 0; fixtures[i] != NULL; i++) {
        if (stat(fixtures[i], &st) != 0) {
            continue; // Fixture doesn't exist yet
        }

        char *content = load_file_to_string(ctx, fixtures[i]);
        ck_assert_ptr_nonnull(content);

        // For JSONL files, validate each line
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
            // For JSON files, validate single object
            yyjson_doc *doc = yyjson_read(content, strlen(content), 0);
            ck_assert_ptr_nonnull(doc);
            yyjson_doc_free(doc);
        }
    }

    talloc_free(ctx);
}

END_TEST

static Suite *google_mock_verification_suite(void)
{
    Suite *s = suite_create("GoogleMockVerification");
    TCase *tc_core = tcase_create("Core");

    // Set longer timeout for real API calls
    tcase_set_timeout(tc_core, 60);

    // Verification tests (only run with VERIFY_MOCKS=1)
    tcase_add_test(tc_core, verify_google_streaming_text);
    tcase_add_test(tc_core, verify_google_streaming_thinking);
    tcase_add_test(tc_core, verify_google_tool_call);
    tcase_add_test(tc_core, verify_google_error_auth);

    // Fixture validation (always runs)
    tcase_add_test(tc_core, validate_fixture_structure);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = google_mock_verification_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
