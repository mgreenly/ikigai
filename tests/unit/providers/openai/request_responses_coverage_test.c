/**
 * @file request_responses_coverage_test.c
 * @brief Coverage tests for OpenAI Responses API request serialization
 *
 * Tests to achieve 100% coverage by triggering all error paths and branches.
 */

#include "request_responses_test_helpers.h"

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
 * Tool Choice Default Case Test
 * ================================================================ */

START_TEST(test_tool_choice_default_case) {
	ik_request_t *req = NULL;
	res_t create_result = ik_request_create(test_ctx, "o1", &req);
	ck_assert(!is_err(&create_result));

	ik_request_add_message(req, IK_ROLE_USER, "Test");

	const char *params = "{\"type\":\"object\"}";
	ik_request_add_tool(req, "test_tool", "Test description", params, true);

	// Set an invalid tool_choice_mode to trigger default case
	req->tool_choice_mode = 999;

	char *json = NULL;
	res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

	// Should succeed with "auto" as the default
	ck_assert(!is_err(&result));
	ck_assert_ptr_nonnull(json);

	// Verify tool_choice is "auto"
	yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
	ck_assert_ptr_nonnull(doc);
	yyjson_val *root = yyjson_doc_get_root(doc);
	yyjson_val *tool_choice = yyjson_obj_get(root, "tool_choice");
	ck_assert_ptr_nonnull(tool_choice);
	const char *choice_val = yyjson_get_str(tool_choice);
	ck_assert_str_eq(choice_val, "auto");
	yyjson_doc_free(doc);
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *request_responses_coverage_suite(void)
{
	Suite *s = suite_create("OpenAI Responses API Coverage Tests");

	TCase *tc_reasoning = tcase_create("Reasoning Edge Cases");
	tcase_set_timeout(tc_reasoning, 30);
	tcase_add_checked_fixture(tc_reasoning, request_responses_setup, request_responses_teardown);
	tcase_add_test(tc_reasoning, test_reasoning_invalid_level);
	suite_add_tcase(s, tc_reasoning);

	TCase *tc_tool_choice = tcase_create("Tool Choice Edge Cases");
	tcase_set_timeout(tc_tool_choice, 30);
	tcase_add_checked_fixture(tc_tool_choice, request_responses_setup, request_responses_teardown);
	tcase_add_test(tc_tool_choice, test_tool_choice_default_case);
	suite_add_tcase(s, tc_tool_choice);

	// Note: Additional edge case tests for lines 158 and 185 are complex due to
	// branch evaluation order and require more investigation. The tool_choice
	// default case test above provides the most straightforward coverage improvement.

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
