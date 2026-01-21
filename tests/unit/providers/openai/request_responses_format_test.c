#include "../../../test_constants.h"
/**
 * @file request_responses_format_test.c
 * @brief Input format, instructions, and output tests for OpenAI Responses API
 */

#include "../../../../src/providers/openai/request.h"
#include "../../../../src/providers/request.h"
#include "../../../../src/providers/provider.h"
#include "../../../../src/error.h"
#include "../../../../src/vendor/yyjson/yyjson.h"

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

/* Test fixture */
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
 * Input Format Tests
 * ================================================================ */

START_TEST(test_serialize_multi_turn_conversation) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    // Add multiple messages
    ik_request_add_message(req, IK_ROLE_USER, "First message");
    ik_request_add_message(req, IK_ROLE_ASSISTANT, "First response");
    ik_request_add_message(req, IK_ROLE_USER, "Second message");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // Multi-turn should use array format
    yyjson_val *input = yyjson_obj_get(root, "input");
    ck_assert_ptr_nonnull(input);
    ck_assert(yyjson_is_arr(input));
    ck_assert_int_eq((int)yyjson_arr_size(input), 3);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_non_user_message) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    // Single assistant message should use array format (not string)
    ik_request_add_message(req, IK_ROLE_ASSISTANT, "Assistant message");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // Non-user message should use array format
    yyjson_val *input = yyjson_obj_get(root, "input");
    ck_assert_ptr_nonnull(input);
    ck_assert(yyjson_is_arr(input));

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_multiple_content_blocks_with_separator) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    // Create multiple text content blocks
    ik_content_block_t *blocks = talloc_array(test_ctx, ik_content_block_t, 2);
    blocks[0] = *ik_content_block_text(test_ctx, "First block");
    blocks[1] = *ik_content_block_text(test_ctx, "Second block");

    res_t result = ik_request_add_message_blocks(req, IK_ROLE_USER, blocks, 2);
    ck_assert(!is_err(&result));

    char *json = NULL;
    res_t serialize_result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&serialize_result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // Single user message with multiple text blocks should use string format with \n\n separator
    yyjson_val *input = yyjson_obj_get(root, "input");
    ck_assert_ptr_nonnull(input);
    ck_assert(yyjson_is_str(input));
    ck_assert_str_eq(yyjson_get_str(input), "First block\n\nSecond block");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_empty_input) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    // Create a user message with only a tool_call block (no text)
    ik_content_block_t *blocks = talloc_array(test_ctx, ik_content_block_t, 1);
    blocks[0] = *ik_content_block_tool_call(test_ctx, "call_123", "test_tool", "{}");

    res_t result = ik_request_add_message_blocks(req, IK_ROLE_USER, blocks, 1);
    ck_assert(!is_err(&result));

    char *json = NULL;
    res_t serialize_result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&serialize_result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // Empty text should result in empty string input
    yyjson_val *input = yyjson_obj_get(root, "input");
    ck_assert_ptr_nonnull(input);
    ck_assert(yyjson_is_str(input));
    ck_assert_str_eq(yyjson_get_str(input), "");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_user_message_with_zero_content_blocks) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    // Create a single user message with content_count == 0
    req->message_count = 1;
    req->messages = talloc_zero_array(req, ik_message_t, 1);
    req->messages[0].role = IK_ROLE_USER;
    req->messages[0].content_count = 0;  // Zero content blocks
    req->messages[0].content_blocks = NULL;

    char *json = NULL;
    res_t serialize_result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&serialize_result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // Should use array format (not string) because content_count == 0
    yyjson_val *input = yyjson_obj_get(root, "input");
    ck_assert_ptr_nonnull(input);
    ck_assert(yyjson_is_arr(input));

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_mixed_content_types_with_text) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    // Create a user message with mixed content: text, tool_call, text
    ik_content_block_t *blocks = talloc_array(test_ctx, ik_content_block_t, 3);
    blocks[0] = *ik_content_block_text(test_ctx, "First text");
    blocks[1] = *ik_content_block_tool_call(test_ctx, "call_123", "test", "{}");
    blocks[2] = *ik_content_block_text(test_ctx, "Second text");

    res_t result = ik_request_add_message_blocks(req, IK_ROLE_USER, blocks, 3);
    ck_assert(!is_err(&result));

    char *json = NULL;
    res_t serialize_result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&serialize_result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // Should concatenate only text blocks, skipping non-text blocks
    yyjson_val *input = yyjson_obj_get(root, "input");
    ck_assert_ptr_nonnull(input);
    ck_assert(yyjson_is_str(input));
    ck_assert_str_eq(yyjson_get_str(input), "First text\n\nSecond text");

    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Instructions (System Prompt) Tests
 * ================================================================ */

START_TEST(test_serialize_with_system_prompt) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    res_t sys_result = ik_request_set_system(req, "You are a helpful assistant.");
    ck_assert(!is_err(&sys_result));
    ik_request_add_message(req, IK_ROLE_USER, "Test");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *instructions = yyjson_obj_get(root, "instructions");
    ck_assert_ptr_nonnull(instructions);
    ck_assert_str_eq(yyjson_get_str(instructions), "You are a helpful assistant.");

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_without_system_prompt) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Test");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // No instructions field when system_prompt is NULL
    yyjson_val *instructions = yyjson_obj_get(root, "instructions");
    ck_assert_ptr_null(instructions);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_with_empty_system_prompt) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    res_t sys_result = ik_request_set_system(req, "");
    ck_assert(!is_err(&sys_result));
    ik_request_add_message(req, IK_ROLE_USER, "Test");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // No instructions field when system_prompt is empty string
    yyjson_val *instructions = yyjson_obj_get(root, "instructions");
    ck_assert_ptr_null(instructions);

    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Streaming and Output Tests
 * ================================================================ */

START_TEST(test_serialize_streaming_enabled) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Test streaming");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, true, &json);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(json);

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *stream = yyjson_obj_get(root, "stream");
    ck_assert_ptr_nonnull(stream);
    ck_assert(yyjson_get_bool(stream));

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_streaming_disabled) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Test no streaming");

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // No stream field when streaming is disabled
    yyjson_val *stream = yyjson_obj_get(root, "stream");
    ck_assert_ptr_null(stream);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_max_output_tokens) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Test");
    req->max_output_tokens = 1024;

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    yyjson_val *max_tokens = yyjson_obj_get(root, "max_output_tokens");
    ck_assert_ptr_nonnull(max_tokens);
    ck_assert_int_eq((int)yyjson_get_int(max_tokens), 1024);

    yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_serialize_no_max_output_tokens) {
    ik_request_t *req = NULL;
    res_t create_result = ik_request_create(test_ctx, "o1", &req);
    ck_assert(!is_err(&create_result));

    ik_request_add_message(req, IK_ROLE_USER, "Test");
    // max_output_tokens defaults to 0 (not set)

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(!is_err(&result));

    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    yyjson_val *root = yyjson_doc_get_root(doc);

    // No max_output_tokens field when not set
    yyjson_val *max_tokens = yyjson_obj_get(root, "max_output_tokens");
    ck_assert_ptr_null(max_tokens);

    yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *request_responses_format_suite(void)
{
    Suite *s = suite_create("OpenAI Responses API Format and Output");

    TCase *tc_input = tcase_create("Input Format");
    tcase_set_timeout(tc_input, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_input, setup, teardown);
    tcase_add_test(tc_input, test_serialize_multi_turn_conversation);
    tcase_add_test(tc_input, test_serialize_non_user_message);
    tcase_add_test(tc_input, test_serialize_multiple_content_blocks_with_separator);
    tcase_add_test(tc_input, test_serialize_empty_input);
    tcase_add_test(tc_input, test_serialize_user_message_with_zero_content_blocks);
    tcase_add_test(tc_input, test_serialize_mixed_content_types_with_text);
    suite_add_tcase(s, tc_input);

    TCase *tc_instructions = tcase_create("Instructions");
    tcase_set_timeout(tc_instructions, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_instructions, setup, teardown);
    tcase_add_test(tc_instructions, test_serialize_with_system_prompt);
    tcase_add_test(tc_instructions, test_serialize_without_system_prompt);
    tcase_add_test(tc_instructions, test_serialize_with_empty_system_prompt);
    suite_add_tcase(s, tc_instructions);

    TCase *tc_streaming = tcase_create("Streaming and Output");
    tcase_set_timeout(tc_streaming, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_streaming, setup, teardown);
    tcase_add_test(tc_streaming, test_serialize_streaming_enabled);
    tcase_add_test(tc_streaming, test_serialize_streaming_disabled);
    tcase_add_test(tc_streaming, test_serialize_max_output_tokens);
    tcase_add_test(tc_streaming, test_serialize_no_max_output_tokens);
    suite_add_tcase(s, tc_streaming);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = request_responses_format_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/providers/openai/request_responses_format_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
