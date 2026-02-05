#include "tests/test_constants.h"
/**
 * @file request_responses_tool_errors_test.c
 * @brief Coverage tests for OpenAI Responses API tool serialization errors
 *
 * Tests to achieve 100% coverage of tool serialization error paths.
 */

#include "request_responses_test_helper.h"

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

    // Fail on yyjson_mut_obj_add_bool_ call (adding "strict")
    yyjson_call_count = 0;
    yyjson_fail_count = 5;

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

    // Fail on yyjson_mut_arr_add_val_ call (adding tool to array)
    yyjson_call_count = 0;
    yyjson_fail_count = 6;

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

    // Fail on yyjson_mut_obj_add_str_ in add_tool_choice
    // This happens after all tool serialization succeeds (6 calls) + 1 for tool_choice
    yyjson_call_count = 0;
    yyjson_fail_count = 7;

    char *json = NULL;
    res_t result = ik_openai_serialize_responses_request(test_ctx, req, false, &json);

    ck_assert(is_err(&result));
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *request_responses_tool_errors_suite(void)
{
    Suite *s = suite_create("OpenAI Responses API Tool Error Tests");

    TCase *tc_tool_errors = tcase_create("Tool Serialization Errors");
    tcase_set_timeout(tc_tool_errors, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_tool_errors, request_responses_setup, request_responses_teardown);
    tcase_add_test(tc_tool_errors, test_serialize_tool_add_type_fails);
    tcase_add_test(tc_tool_errors, test_serialize_tool_add_name_fails);
    tcase_add_test(tc_tool_errors, test_serialize_tool_add_description_fails);
    tcase_add_test(tc_tool_errors, test_serialize_tool_add_parameters_fails);
    tcase_add_test(tc_tool_errors, test_serialize_tool_add_strict_fails);
    tcase_add_test(tc_tool_errors, test_serialize_tool_add_to_array_fails);
    suite_add_tcase(s, tc_tool_errors);

    TCase *tc_tool_choice = tcase_create("Tool Choice Errors");
    tcase_set_timeout(tc_tool_choice, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_tool_choice, request_responses_setup, request_responses_teardown);
    tcase_add_test(tc_tool_choice, test_add_tool_choice_fails);
    suite_add_tcase(s, tc_tool_choice);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = request_responses_tool_errors_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/openai/request_responses_tool_errors_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
