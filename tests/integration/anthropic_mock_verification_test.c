#include "anthropic_mock_verification_helper.h"

#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>
#include "../../src/vendor/yyjson/yyjson.h"
#include "../test_utils_helper.h"

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

START_TEST(verify_anthropic_streaming_text) {
    // Skip if not in verification mode
    if (!should_verify_mocks_anthropic()) {
        ck_assert(true);
        return;
    }

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    const char *api_key = get_api_key_anthropic(ctx);
    if (!api_key) {
        ck_abort_msg("ANTHROPIC_API_KEY not set");
    }

    // Build request
    const char *request_body =
        "{"
        "\"model\":\"claude-sonnet-4-5-20250929\","
        "\"max_tokens\":100,"
        "\"stream\":true,"
        "\"messages\":[{\"role\":\"user\",\"content\":\"Say hello\"}]"
        "}";

    // Make API call
    sse_event_accumulator_t *acc = create_sse_event_accumulator(ctx);
    int status = http_post_sse_anthropic(ctx, "https://api.anthropic.com/v1/messages",
                                         api_key, request_body, acc);

    // Verify HTTP status
    ck_assert_int_eq(status, 200);
    ck_assert(acc->count > 0);

    // Parse events and verify structure
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

    // Verify event order
    ck_assert(seen_message_start);
    ck_assert(seen_content_block_start);
    ck_assert(seen_content_block_delta);
    ck_assert(seen_content_block_stop);
    ck_assert(seen_message_delta);
    ck_assert(seen_message_stop);

    // Optionally capture fixture
    capture_fixture_anthropic("stream_text_basic", acc);

    talloc_free(ctx);
}

END_TEST

START_TEST(verify_anthropic_streaming_thinking) {
    // Skip if not in verification mode
    if (!should_verify_mocks_anthropic()) {
        ck_assert(true);
        return;
    }

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    const char *api_key = get_api_key_anthropic(ctx);
    if (!api_key) {
        ck_abort_msg("ANTHROPIC_API_KEY not set");
    }

    // Build request with thinking enabled
    const char *request_body =
        "{"
        "\"model\":\"claude-sonnet-4-5-20250929\","
        "\"max_tokens\":1000,"
        "\"stream\":true,"
        "\"thinking\":{\"type\":\"enabled\",\"budget_tokens\":500},"
        "\"messages\":[{\"role\":\"user\",\"content\":\"What is 15 * 17?\"}]"
        "}";

    // Make API call
    sse_event_accumulator_t *acc = create_sse_event_accumulator(ctx);
    int status = http_post_sse_anthropic(ctx, "https://api.anthropic.com/v1/messages",
                                         api_key, request_body, acc);

    // Verify HTTP status
    ck_assert_int_eq(status, 200);
    ck_assert(acc->count > 0);

    // Parse events and verify thinking block
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

    // Verify thinking structure
    ck_assert(seen_thinking_start);
    ck_assert(seen_thinking_delta);
    ck_assert(seen_text_start);
    ck_assert_int_eq(thinking_index, 0);
    ck_assert_int_eq(text_index, 1);

    // Optionally capture fixture
    capture_fixture_anthropic("stream_text_thinking", acc);

    talloc_free(ctx);
}

END_TEST

START_TEST(verify_anthropic_tool_call) {
    // Skip if not in verification mode
    if (!should_verify_mocks_anthropic()) {
        ck_assert(true);
        return;
    }

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    const char *api_key = get_api_key_anthropic(ctx);
    if (!api_key) {
        ck_abort_msg("ANTHROPIC_API_KEY not set");
    }

    // Build request with tool definition
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

    // Make API call
    sse_event_accumulator_t *acc = create_sse_event_accumulator(ctx);
    int status = http_post_sse_anthropic(ctx, "https://api.anthropic.com/v1/messages",
                                         api_key, request_body, acc);

    // Verify HTTP status
    ck_assert_int_eq(status, 200);
    ck_assert(acc->count > 0);

    // Parse events and verify tool use
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

    // Verify tool call structure
    ck_assert(seen_tool_use);
    ck_assert(seen_input_json_delta);
    ck_assert_ptr_nonnull(tool_id);
    if (stop_reason) {
        ck_assert_str_eq(stop_reason, "tool_use");
    }

    // Optionally capture fixture
    capture_fixture_anthropic("stream_tool_call", acc);

    talloc_free(ctx);
}

END_TEST

START_TEST(verify_anthropic_error_auth) {
    // Skip if not in verification mode
    if (!should_verify_mocks_anthropic()) {
        ck_assert(true);
        return;
    }

    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Use invalid API key
    const char *invalid_key = "sk-ant-invalid";

    const char *request_body =
        "{"
        "\"model\":\"claude-sonnet-4-5-20250929\","
        "\"max_tokens\":100,"
        "\"messages\":[{\"role\":\"user\",\"content\":\"Hello\"}]"
        "}";

    // Make API call (should fail)
    char *response = NULL;
    int status = http_post_json_anthropic(ctx, "https://api.anthropic.com/v1/messages",
                                          invalid_key, request_body, &response);

    // Verify HTTP status 401
    ck_assert_int_eq(status, 401);
    ck_assert_ptr_nonnull(response);

    // Parse error response
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

    // Optionally capture fixture
    if (should_capture_fixtures_anthropic()) {
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

END_TEST

START_TEST(validate_fixture_structure) {
    // This test runs even without VERIFY_MOCKS to validate fixture files
    TALLOC_CTX *ctx = talloc_new(NULL);
    ck_assert_ptr_nonnull(ctx);

    // Check if fixtures exist - if not, skip test
    struct stat st;
    if (stat("tests/fixtures/vcr/anthropic/stream_text_basic.jsonl", &st) != 0) {
        // Fixtures not yet created - skip
        talloc_free(ctx);
        return;
    }

    // Validate each fixture has correct JSON structure
    const char *fixtures[] = {
        "tests/fixtures/vcr/anthropic/stream_text_basic.jsonl",
        "tests/fixtures/vcr/anthropic/stream_text_thinking.jsonl",
        "tests/fixtures/vcr/anthropic/stream_tool_call.jsonl",
        "tests/fixtures/vcr/anthropic/error_401_auth.json",
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

static Suite *anthropic_mock_verification_suite(void)
{
    Suite *s = suite_create("AnthropicMockVerification");
    TCase *tc_core = tcase_create("Core");

    // Set longer timeout for real API calls
    tcase_set_timeout(tc_core, IK_TEST_TIMEOUT);

    // Verification tests (only run with VERIFY_MOCKS=1)
    tcase_add_test(tc_core, verify_anthropic_streaming_text);
    tcase_add_test(tc_core, verify_anthropic_streaming_thinking);
    tcase_add_test(tc_core, verify_anthropic_tool_call);
    tcase_add_test(tc_core, verify_anthropic_error_auth);

    // Fixture validation (always runs)
    tcase_add_test(tc_core, validate_fixture_structure);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = anthropic_mock_verification_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/integration/anthropic_mock_verification_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
