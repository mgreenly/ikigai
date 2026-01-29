#include "../../../test_constants.h"
/**
 * @file test_google_client.c
 * @brief Unit tests for Google request serialization
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/google/request.h"
#include "providers/provider.h"
#include "providers/request.h"

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * Request Serialization Tests
 * ================================================================ */

START_TEST(test_build_request_with_system_and_user_messages) {
    // Create a basic request
    ik_request_t *req = talloc_zero(test_ctx, ik_request_t);
    req->model = talloc_strdup(req, "gemini-2.5-flash");
    req->max_output_tokens = 1024;

    // Add system prompt
    req->system_prompt = talloc_strdup(req, "You are a helpful assistant.");

    // Add user message
    req->messages = talloc_zero_array(req, ik_message_t, 1);
    req->message_count = 1;
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_blocks = talloc_zero_array(req, ik_content_block_t, 1);
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(req, "Hello!");

    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    // Verify structure contains expected fields
    ck_assert_ptr_nonnull(strstr(json, "systemInstruction"));
    ck_assert_ptr_nonnull(strstr(json, "You are a helpful assistant"));
    ck_assert_ptr_nonnull(strstr(json, "contents"));
    ck_assert_ptr_nonnull(strstr(json, "Hello!"));
}
END_TEST

START_TEST(test_build_request_gemini_2_5_with_thinking_budget) {
    ik_request_t *req = talloc_zero(test_ctx, ik_request_t);
    req->model = talloc_strdup(req, "gemini-2.5-pro");
    req->max_output_tokens = 1024;

    // Set thinking configuration
    req->thinking.level = IK_THINKING_HIGH;

    // Add user message
    req->messages = talloc_zero_array(req, ik_message_t, 1);
    req->message_count = 1;
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_blocks = talloc_zero_array(req, ik_content_block_t, 1);
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(req, "Solve this problem.");

    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    // Verify thinking budget is present for Gemini 2.5
    ck_assert_ptr_nonnull(strstr(json, "generationConfig"));
    ck_assert_ptr_nonnull(strstr(json, "thinkingConfig"));
    ck_assert_ptr_nonnull(strstr(json, "thinkingBudget"));
}

END_TEST

START_TEST(test_build_request_gemini_3_with_thinking_level) {
    ik_request_t *req = talloc_zero(test_ctx, ik_request_t);
    req->model = talloc_strdup(req, "gemini-3-pro");
    req->max_output_tokens = 1024;

    // Set thinking configuration
    req->thinking.level = IK_THINKING_HIGH;

    // Add user message
    req->messages = talloc_zero_array(req, ik_message_t, 1);
    req->message_count = 1;
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_blocks = talloc_zero_array(req, ik_content_block_t, 1);
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(req, "Solve this problem.");

    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    // Verify thinking level is present for Gemini 3
    ck_assert_ptr_nonnull(strstr(json, "generationConfig"));
    ck_assert_ptr_nonnull(strstr(json, "thinkingConfig"));
    ck_assert_ptr_nonnull(strstr(json, "thinkingLevel"));
    ck_assert_ptr_nonnull(strstr(json, "high"));
}

END_TEST

START_TEST(test_build_request_with_tool_declarations) {
    ik_request_t *req = talloc_zero(test_ctx, ik_request_t);
    req->model = talloc_strdup(req, "gemini-2.5-flash");
    req->max_output_tokens = 1024;

    // Add tool declarations
    req->tools = talloc_zero_array(req, ik_tool_def_t, 1);
    req->tool_count = 1;
    req->tools[0].name = talloc_strdup(req, "get_weather");
    req->tools[0].description = talloc_strdup(req, "Get the weather");
    req->tools[0].parameters = talloc_strdup(req, "{\"type\":\"object\"}");

    // Add user message
    req->messages = talloc_zero_array(req, ik_message_t, 1);
    req->message_count = 1;
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_blocks = talloc_zero_array(req, ik_content_block_t, 1);
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(req, "What's the weather?");

    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    // Verify tools are present
    ck_assert_ptr_nonnull(strstr(json, "tools"));
    ck_assert_ptr_nonnull(strstr(json, "get_weather"));
}

END_TEST

START_TEST(test_build_request_without_optional_fields) {
    ik_request_t *req = talloc_zero(test_ctx, ik_request_t);
    req->model = talloc_strdup(req, "gemini-2.5-flash");
    req->max_output_tokens = 1024;

    // Only user message, no system prompt, no tools
    req->messages = talloc_zero_array(req, ik_message_t, 1);
    req->message_count = 1;
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_blocks = talloc_zero_array(req, ik_content_block_t, 1);
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(req, "Hello!");

    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    // Verify minimal structure
    ck_assert_ptr_nonnull(strstr(json, "contents"));
    ck_assert_ptr_nonnull(strstr(json, "Hello!"));

    // Optional fields should not be present
    ck_assert_ptr_null(strstr(json, "systemInstruction"));
    ck_assert_ptr_null(strstr(json, "tools"));
}

END_TEST

START_TEST(test_api_key_in_url) {
    char *url = NULL;
    res_t r = ik_google_build_url(test_ctx, "https://generativelanguage.googleapis.com/v1beta",
                                  "gemini-2.5-flash", "test-key-12345", false, &url);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(url);

    // Verify URL structure: base/models/model:generateContent?key=api_key
    ck_assert_ptr_nonnull(strstr(url, "models/gemini-2.5-flash:generateContent"));
    ck_assert_ptr_nonnull(strstr(url, "key=test-key-12345"));
}

END_TEST

START_TEST(test_json_structure_matches_gemini_api) {
    ik_request_t *req = talloc_zero(test_ctx, ik_request_t);
    req->model = talloc_strdup(req, "gemini-2.5-flash");
    req->max_output_tokens = 1024;

    // Add user message
    req->messages = talloc_zero_array(req, ik_message_t, 1);
    req->message_count = 1;
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_blocks = talloc_zero_array(req, ik_content_block_t, 1);
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(req, "Hello!");

    char *json = NULL;
    res_t r = ik_google_serialize_request(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    // Verify Google API structure
    ck_assert_ptr_nonnull(strstr(json, "contents"));
    ck_assert_ptr_nonnull(strstr(json, "role"));
    ck_assert_ptr_nonnull(strstr(json, "user"));
    ck_assert_ptr_nonnull(strstr(json, "parts"));
    ck_assert_ptr_nonnull(strstr(json, "text"));
    ck_assert_ptr_nonnull(strstr(json, "Hello!"));
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_client_suite(void)
{
    Suite *s = suite_create("Google Client");

    TCase *tc_serialize = tcase_create("Request Serialization");
    tcase_set_timeout(tc_serialize, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_serialize, setup, teardown);
    tcase_add_test(tc_serialize, test_build_request_with_system_and_user_messages);
    tcase_add_test(tc_serialize, test_build_request_gemini_2_5_with_thinking_budget);
    tcase_add_test(tc_serialize, test_build_request_gemini_3_with_thinking_level);
    tcase_add_test(tc_serialize, test_build_request_with_tool_declarations);
    tcase_add_test(tc_serialize, test_build_request_without_optional_fields);
    tcase_add_test(tc_serialize, test_api_key_in_url);
    tcase_add_test(tc_serialize, test_json_structure_matches_gemini_api);
    suite_add_tcase(s, tc_serialize);

    return s;
}

int main(void)
{
    Suite *s = google_client_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/providers/google/google_client_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
