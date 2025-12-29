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
