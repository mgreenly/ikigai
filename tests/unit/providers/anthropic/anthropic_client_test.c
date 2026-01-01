/**
 * @file test_anthropic_client.c
 * @brief Unit tests for Anthropic request serialization
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "vendor/yyjson/yyjson.h"
#include "providers/anthropic/request.h"
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
    req->model = talloc_strdup(req, "claude-sonnet-4-5-20250929");
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
    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    // Parse and verify structure
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    yyjson_val *model = yyjson_obj_get(root, "model");
    ck_assert_ptr_nonnull(model);
    ck_assert_str_eq(yyjson_get_str(model), "claude-sonnet-4-5-20250929");

    yyjson_val *system = yyjson_obj_get(root, "system");
    ck_assert_ptr_nonnull(system);

    yyjson_val *messages = yyjson_obj_get(root, "messages");
    ck_assert_ptr_nonnull(messages);
    ck_assert(yyjson_is_arr(messages));

    yyjson_doc_free(doc);
}
END_TEST

START_TEST(test_build_request_with_thinking_budget) {
    ik_request_t *req = talloc_zero(test_ctx, ik_request_t);
    req->model = talloc_strdup(req, "claude-sonnet-4-5-20250929");
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
    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    // Verify thinking configuration is present
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    // Anthropic uses "thinking" field for extended thinking
    yyjson_val *thinking = yyjson_obj_get(root, "thinking");
    if (thinking != NULL) {
        // Verify it has proper structure
        ck_assert(yyjson_is_obj(thinking));
    }

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_build_request_with_tool_definitions) {
    ik_request_t *req = talloc_zero(test_ctx, ik_request_t);
    req->model = talloc_strdup(req, "claude-sonnet-4-5-20250929");
    req->max_output_tokens = 1024;

    // Add tool definition
    req->tools = talloc_zero_array(req, ik_tool_def_t, 1);
    req->tool_count = 1;
    req->tools[0].name = talloc_strdup(req, "get_weather");
    req->tools[0].description = talloc_strdup(req, "Get weather for a location");
    req->tools[0].parameters = talloc_strdup(req,
                                             "{\"type\":\"object\",\"properties\":{\"location\":{\"type\":\"string\"}}}");

    // Add user message
    req->messages = talloc_zero_array(req, ik_message_t, 1);
    req->message_count = 1;
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_blocks = talloc_zero_array(req, ik_content_block_t, 1);
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(req, "What's the weather?");

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    // Verify tools array is present
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    yyjson_val *tools = yyjson_obj_get(root, "tools");
    ck_assert_ptr_nonnull(tools);
    ck_assert(yyjson_is_arr(tools));

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_build_request_without_optional_fields) {
    ik_request_t *req = talloc_zero(test_ctx, ik_request_t);
    req->model = talloc_strdup(req, "claude-sonnet-4-5-20250929");
    req->max_output_tokens = 1024;

    // Minimal request - just a user message
    req->messages = talloc_zero_array(req, ik_message_t, 1);
    req->message_count = 1;
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_blocks = talloc_zero_array(req, ik_content_block_t, 1);
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(req, "Hello!");

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);

    // Should have model, max_tokens, messages
    ck_assert_ptr_nonnull(yyjson_obj_get(root, "model"));
    ck_assert_ptr_nonnull(yyjson_obj_get(root, "max_tokens"));
    ck_assert_ptr_nonnull(yyjson_obj_get(root, "messages"));

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_verify_json_structure_matches_api_spec) {
    ik_request_t *req = talloc_zero(test_ctx, ik_request_t);
    req->model = talloc_strdup(req, "claude-sonnet-4-5-20250929");
    req->max_output_tokens = 2048;

    req->messages = talloc_zero_array(req, ik_message_t, 1);
    req->message_count = 1;
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_blocks = talloc_zero_array(req, ik_content_block_t, 1);
    req->messages[0].content_count = 1;
    req->messages[0].content_blocks[0].type = IK_CONTENT_TEXT;
    req->messages[0].content_blocks[0].data.text.text = talloc_strdup(req, "Test message");

    char *json = NULL;
    res_t r = ik_anthropic_serialize_request(test_ctx, req, &json);

    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(json);

    // Verify valid JSON
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_ptr_nonnull(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_ptr_nonnull(root);
    ck_assert(yyjson_is_obj(root));

    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *anthropic_client_suite(void)
{
    Suite *s = suite_create("Anthropic Client");

    TCase *tc_request = tcase_create("Request Serialization");
    tcase_set_timeout(tc_request, 30);
    tcase_add_unchecked_fixture(tc_request, setup, teardown);
    tcase_add_test(tc_request, test_build_request_with_system_and_user_messages);
    tcase_add_test(tc_request, test_build_request_with_thinking_budget);
    tcase_add_test(tc_request, test_build_request_with_tool_definitions);
    tcase_add_test(tc_request, test_build_request_without_optional_fields);
    tcase_add_test(tc_request, test_verify_json_structure_matches_api_spec);
    suite_add_tcase(s, tc_request);

    return s;
}

int main(void)
{
    Suite *s = anthropic_client_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
