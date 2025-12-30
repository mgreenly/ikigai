/**
 * @file request_responses_coverage_test.c
 * @brief Coverage tests for OpenAI Responses API request serialization
 *
 * Tests to achieve 100% coverage by triggering all error paths and branches.
 */

#include "../../../../src/providers/openai/request.h"
#include "../../../../src/providers/request.h"
#include "../../../../src/providers/provider.h"
#include "../../../../src/error.h"
#include "../../../../src/wrapper.h"
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
 * Mock Helpers for Triggering yyjson Failures
 * ================================================================ */

static int32_t yyjson_fail_count = 0;
static int32_t yyjson_call_count = 0;

// Mock yyjson_mut_obj_add_str_ wrapper
bool yyjson_mut_obj_add_str_(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                              const char *key, const char *val)
{
	yyjson_call_count++;
	if (yyjson_fail_count > 0 && yyjson_call_count == yyjson_fail_count) {
		return false;
	}
	// Call real implementation
	return yyjson_mut_obj_add_str(doc, obj, key, val);
}

// Mock yyjson_mut_obj_add_val_ wrapper
bool yyjson_mut_obj_add_val_(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                              const char *key, yyjson_mut_val *val)
{
	yyjson_call_count++;
	if (yyjson_fail_count > 0 && yyjson_call_count == yyjson_fail_count) {
		return false;
	}
	// Call real implementation
	return yyjson_mut_obj_add_val(doc, obj, key, val);
}

// Mock yyjson_mut_obj_add_bool_ wrapper
bool yyjson_mut_obj_add_bool_(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                               const char *key, bool val)
{
	yyjson_call_count++;
	if (yyjson_fail_count > 0 && yyjson_call_count == yyjson_fail_count) {
		return false;
	}
	// Call real implementation
	return yyjson_mut_obj_add_bool(doc, obj, key, val);
}

// Mock yyjson_mut_arr_add_val_ wrapper
bool yyjson_mut_arr_add_val_(yyjson_mut_val *arr, yyjson_mut_val *val)
{
	yyjson_call_count++;
	if (yyjson_fail_count > 0 && yyjson_call_count == yyjson_fail_count) {
		return false;
	}
	// Call real implementation
	return yyjson_mut_arr_add_val(arr, val);
}

/* ================================================================
 * serialize_responses_tool Error Path Tests
 * ================================================================ */

START_TEST(test_serialize_tool_add_type_fails) {
	ik_request_t *req = NULL;
	res_t create_result = ik_request_create(test_ctx, "o1", &req);
	ck_assert(!is_err(&create_result));

	ik_request_add_message(req, IK_ROLE_USER, "Test");

	const char *params = "{\"type\":\"object\"}";
	ik_request_add_tool(req, "test_tool", "Test description", params, true);

	// Fail on first yyjson_mut_obj_add_str_ call (line 35: adding "type")
	yyjson_call_count = 0;
	yyjson_fail_count = 1;

	char *json = NULL;
	res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

	ck_assert(is_err(&result));
}

END_TEST

START_TEST(test_serialize_tool_add_name_fails) {
	ik_request_t *req = NULL;
	res_t create_result = ik_request_create(test_ctx, "o1", &req);
	ck_assert(!is_err(&create_result));

	ik_request_add_message(req, IK_ROLE_USER, "Test");

	const char *params = "{\"type\":\"object\"}";
	ik_request_add_tool(req, "test_tool", "Test description", params, true);

	// Fail on second yyjson_mut_obj_add_str_ call (line 44: adding "name")
	yyjson_call_count = 0;
	yyjson_fail_count = 2;

	char *json = NULL;
	res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

	ck_assert(is_err(&result));
}

END_TEST

START_TEST(test_serialize_tool_add_description_fails) {
	ik_request_t *req = NULL;
	res_t create_result = ik_request_create(test_ctx, "o1", &req);
	ck_assert(!is_err(&create_result));

	ik_request_add_message(req, IK_ROLE_USER, "Test");

	const char *params = "{\"type\":\"object\"}";
	ik_request_add_tool(req, "test_tool", "Test description", params, true);

	// Fail on third yyjson_mut_obj_add_str_ call (line 49: adding "description")
	yyjson_call_count = 0;
	yyjson_fail_count = 3;

	char *json = NULL;
	res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

	ck_assert(is_err(&result));
}

END_TEST

START_TEST(test_serialize_tool_add_parameters_fails) {
	ik_request_t *req = NULL;
	res_t create_result = ik_request_create(test_ctx, "o1", &req);
	ck_assert(!is_err(&create_result));

	ik_request_add_message(req, IK_ROLE_USER, "Test");

	const char *params = "{\"type\":\"object\"}";
	ik_request_add_tool(req, "test_tool", "Test description", params, true);

	// Fail on first yyjson_mut_obj_add_val_ call (line 62: adding "parameters")
	yyjson_call_count = 0;
	yyjson_fail_count = 4;

	char *json = NULL;
	res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

	ck_assert(is_err(&result));
}

END_TEST

START_TEST(test_serialize_tool_add_strict_fails) {
	ik_request_t *req = NULL;
	res_t create_result = ik_request_create(test_ctx, "o1", &req);
	ck_assert(!is_err(&create_result));

	ik_request_add_message(req, IK_ROLE_USER, "Test");

	const char *params = "{\"type\":\"object\"}";
	ik_request_add_tool(req, "test_tool", "Test description", params, true);

	// Fail on yyjson_mut_obj_add_bool_ call (line 67: adding "strict")
	yyjson_call_count = 0;
	yyjson_fail_count = 5;

	char *json = NULL;
	res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

	ck_assert(is_err(&result));
}

END_TEST

START_TEST(test_serialize_tool_add_function_fails) {
	ik_request_t *req = NULL;
	res_t create_result = ik_request_create(test_ctx, "o1", &req);
	ck_assert(!is_err(&create_result));

	ik_request_add_message(req, IK_ROLE_USER, "Test");

	const char *params = "{\"type\":\"object\"}";
	ik_request_add_tool(req, "test_tool", "Test description", params, true);

	// Fail on second yyjson_mut_obj_add_val_ call (line 72: adding "function")
	yyjson_call_count = 0;
	yyjson_fail_count = 6;

	char *json = NULL;
	res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

	ck_assert(is_err(&result));
}

END_TEST

START_TEST(test_serialize_tool_add_to_array_fails) {
	ik_request_t *req = NULL;
	res_t create_result = ik_request_create(test_ctx, "o1", &req);
	ck_assert(!is_err(&create_result));

	ik_request_add_message(req, IK_ROLE_USER, "Test");

	const char *params = "{\"type\":\"object\"}";
	ik_request_add_tool(req, "test_tool", "Test description", params, true);

	// Fail on yyjson_mut_arr_add_val_ call (line 77: adding tool to array)
	yyjson_call_count = 0;
	yyjson_fail_count = 7;

	char *json = NULL;
	res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

	ck_assert(is_err(&result));
}

END_TEST

/* ================================================================
 * add_tool_choice Error Path Tests
 * ================================================================ */

START_TEST(test_add_tool_choice_fails) {
	ik_request_t *req = NULL;
	res_t create_result = ik_request_create(test_ctx, "o1", &req);
	ck_assert(!is_err(&create_result));

	ik_request_add_message(req, IK_ROLE_USER, "Test");

	const char *params = "{\"type\":\"object\"}";
	ik_request_add_tool(req, "test_tool", "Test description", params, true);

	// Fail on yyjson_mut_obj_add_str_ in add_tool_choice (line 108)
	// This happens after all tool serialization succeeds (7 calls) + 1 for tool_choice
	yyjson_call_count = 0;
	yyjson_fail_count = 8;

	char *json = NULL;
	res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

	ck_assert(is_err(&result));
}

END_TEST

/* ================================================================
 * Reasoning Invalid Level Test
 * ================================================================ */

START_TEST(test_reasoning_invalid_level) {
	ik_request_t *req = NULL;
	res_t create_result = ik_request_create(test_ctx, "o1", &req);
	ck_assert(!is_err(&create_result));

	ik_request_add_message(req, IK_ROLE_USER, "Test");

	// Set an invalid thinking level (not 0, 1, 2, or 3)
	req->thinking.level = 999;

	char *json = NULL;
	res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

	// Should succeed but reasoning block should be omitted
	ck_assert(!is_err(&result));
	ck_assert_ptr_nonnull(json);

	// Verify no reasoning field in JSON
	yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
	ck_assert_ptr_nonnull(doc);
	yyjson_val *root = yyjson_doc_get_root(doc);
	yyjson_val *reasoning = yyjson_obj_get(root, "reasoning");
	ck_assert_ptr_null(reasoning);
	yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Single User Message with Non-Text Content
 * ================================================================ */

START_TEST(test_single_message_with_non_text_content) {
	ik_request_t *req = NULL;
	res_t create_result = ik_request_create(test_ctx, "o1", &req);
	ck_assert(!is_err(&create_result));

	// Create a user message with mixed text and non-text content
	// This tests the branches where content_blocks[i].type != IK_CONTENT_TEXT
	ik_message_t *msg = talloc(req, ik_message_t);
	msg->role = IK_ROLE_USER;
	msg->content_count = 2;
	msg->content_blocks = talloc_array(msg, ik_content_block_t, 2);

	// First block: text
	msg->content_blocks[0].type = IK_CONTENT_TEXT;
	msg->content_blocks[0].data.text.text = talloc_strdup(msg, "Hello");

	// Second block: thinking (non-text)
	msg->content_blocks[1].type = IK_CONTENT_THINKING;
	msg->content_blocks[1].data.thinking.text = talloc_strdup(msg, "Some reasoning");

	req->message_count = 1;
	req->messages = talloc_array(req, ik_message_t, 1);
	req->messages[0] = *msg;
	talloc_free(msg);

	char *json = NULL;
	res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

	// Should succeed - non-text blocks are simply skipped in string concatenation
	ck_assert(!is_err(&result));
	ck_assert_ptr_nonnull(json);

	// Verify input field contains only the text content
	yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
	ck_assert_ptr_nonnull(doc);
	yyjson_val *root = yyjson_doc_get_root(doc);
	yyjson_val *input = yyjson_obj_get(root, "input");
	ck_assert_ptr_nonnull(input);
	const char *input_str = yyjson_get_str(input);
	ck_assert_str_eq(input_str, "Hello");
	yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_single_message_only_non_text_content) {
	ik_request_t *req = NULL;
	res_t create_result = ik_request_create(test_ctx, "o1", &req);
	ck_assert(!is_err(&create_result));

	// Create a message with NO text content - only non-text blocks
	ik_message_t *msg = talloc(req, ik_message_t);
	msg->role = IK_ROLE_USER;
	msg->content_count = 1;
	msg->content_blocks = talloc_array(msg, ik_content_block_t, 1);
	msg->content_blocks[0].type = IK_CONTENT_THINKING;
	msg->content_blocks[0].data.thinking.text = talloc_strdup(msg, "Thinking only");

	req->message_count = 1;
	req->messages = talloc_array(req, ik_message_t, 1);
	req->messages[0] = *msg;
	talloc_free(msg);

	char *json = NULL;
	res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

	// Should succeed with empty input string (total_len == 0 case)
	ck_assert(!is_err(&result));
	ck_assert_ptr_nonnull(json);

	// Verify input field is empty string
	yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
	ck_assert_ptr_nonnull(doc);
	yyjson_val *root = yyjson_doc_get_root(doc);
	yyjson_val *input = yyjson_obj_get(root, "input");
	ck_assert_ptr_nonnull(input);
	const char *input_str = yyjson_get_str(input);
	ck_assert_str_eq(input_str, "");
	yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * use_string_input Edge Cases
 * ================================================================ */

START_TEST(test_single_assistant_message) {
	ik_request_t *req = NULL;
	res_t create_result = ik_request_create(test_ctx, "o1", &req);
	ck_assert(!is_err(&create_result));

	// Create a single ASSISTANT message (not USER)
	// This tests use_string_input = false branch (line 157: role != IK_ROLE_USER)
	ik_request_add_message(req, IK_ROLE_ASSISTANT, "I am an assistant");

	char *json = NULL;
	res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

	// Should succeed, using array format (not string)
	ck_assert(!is_err(&result));
	ck_assert_ptr_nonnull(json);

	// Verify input is an array (not a string)
	yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
	ck_assert_ptr_nonnull(doc);
	yyjson_val *root = yyjson_doc_get_root(doc);
	yyjson_val *input = yyjson_obj_get(root, "input");
	ck_assert_ptr_nonnull(input);
	ck_assert(yyjson_is_arr(input));
	yyjson_doc_free(doc);
}

END_TEST

START_TEST(test_single_user_message_empty) {
	ik_request_t *req = NULL;
	res_t create_result = ik_request_create(test_ctx, "o1", &req);
	ck_assert(!is_err(&create_result));

	// Create a single USER message with no content
	// This tests use_string_input = false branch (line 158: content_count == 0)
	ik_message_t *msg = talloc(req, ik_message_t);
	msg->role = IK_ROLE_USER;
	msg->content_count = 0;
	msg->content_blocks = NULL;

	req->message_count = 1;
	req->messages = talloc_array(req, ik_message_t, 1);
	req->messages[0] = *msg;
	talloc_free(msg);

	char *json = NULL;
	res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

	// Should succeed, using array format (not string)
	ck_assert(!is_err(&result));
	ck_assert_ptr_nonnull(json);

	// Verify input is an array (not a string)
	yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
	ck_assert_ptr_nonnull(doc);
	yyjson_val *root = yyjson_doc_get_root(doc);
	yyjson_val *input = yyjson_obj_get(root, "input");
	ck_assert_ptr_nonnull(input);
	ck_assert(yyjson_is_arr(input));
	yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *request_responses_coverage_suite(void)
{
	Suite *s = suite_create("OpenAI Responses API Coverage Tests");

	TCase *tc_tool_errors = tcase_create("Tool Serialization Errors");
    tcase_set_timeout(tc_tool_errors, 30);
	tcase_add_checked_fixture(tc_tool_errors, setup, teardown);
	tcase_add_test(tc_tool_errors, test_serialize_tool_add_type_fails);
	tcase_add_test(tc_tool_errors, test_serialize_tool_add_name_fails);
	tcase_add_test(tc_tool_errors, test_serialize_tool_add_description_fails);
	tcase_add_test(tc_tool_errors, test_serialize_tool_add_parameters_fails);
	tcase_add_test(tc_tool_errors, test_serialize_tool_add_strict_fails);
	tcase_add_test(tc_tool_errors, test_serialize_tool_add_function_fails);
	tcase_add_test(tc_tool_errors, test_serialize_tool_add_to_array_fails);
	suite_add_tcase(s, tc_tool_errors);

	TCase *tc_tool_choice = tcase_create("Tool Choice Errors");
    tcase_set_timeout(tc_tool_choice, 30);
	tcase_add_checked_fixture(tc_tool_choice, setup, teardown);
	tcase_add_test(tc_tool_choice, test_add_tool_choice_fails);
	suite_add_tcase(s, tc_tool_choice);

	TCase *tc_reasoning = tcase_create("Reasoning Edge Cases");
    tcase_set_timeout(tc_reasoning, 30);
	tcase_add_checked_fixture(tc_reasoning, setup, teardown);
	tcase_add_test(tc_reasoning, test_reasoning_invalid_level);
	suite_add_tcase(s, tc_reasoning);

	TCase *tc_content = tcase_create("Non-Text Content Blocks");
    tcase_set_timeout(tc_content, 30);
	tcase_add_checked_fixture(tc_content, setup, teardown);
	tcase_add_test(tc_content, test_single_message_with_non_text_content);
	tcase_add_test(tc_content, test_single_message_only_non_text_content);
	suite_add_tcase(s, tc_content);

	TCase *tc_input_format = tcase_create("Input Format Edge Cases");
    tcase_set_timeout(tc_input_format, 30);
	tcase_add_checked_fixture(tc_input_format, setup, teardown);
	tcase_add_test(tc_input_format, test_single_assistant_message);
	tcase_add_test(tc_input_format, test_single_user_message_empty);
	suite_add_tcase(s, tc_input_format);

	return s;
}

int main(void)
{
	int32_t number_failed;
	Suite *s = request_responses_coverage_suite();
	SRunner *sr = srunner_create(s);

	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
