#include "tests/test_constants.h"
/**
 * @file response_chat_coverage_branches_test.c
 * @brief Additional branch coverage tests for OpenAI Chat response parsing
 *
 * Tests remaining uncovered branches to achieve 100% branch coverage.
 */

#include "apps/ikigai/providers/openai/response.h"
#include "apps/ikigai/providers/provider.h"
#include "shared/error.h"

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
 * Edge Case Branch Coverage Tests
 * ================================================================ */

START_TEST(test_parse_error_empty_json_len) {
    /* Line 344 branch 3: json != NULL but json_len == 0 */
    const char *json = "{\"error\":{\"message\":\"test\"}}";

    ik_error_category_t category;
    char *message = NULL;
    res_t result = ik_openai_parse_error(test_ctx, 500, json, 0,
                                         &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
    /* Should fall back to HTTP status when json_len is 0 */
    ck_assert_str_eq(message, "HTTP 500");
}

END_TEST

START_TEST(test_parse_error_null_type_val) {
    /* Line 361 branch: type_val is NULL (missing type field) */
    const char *json = "{"
                       "\"error\":{"
                       "\"code\":\"test_code\","
                       "\"message\":\"Test message\""
                       "}"
                       "}";

    ik_error_category_t category;
    char *message = NULL;
    res_t result = ik_openai_parse_error(test_ctx, 500, json, strlen(json),
                                         &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
    /* Should use message only when type is missing */
    ck_assert_str_eq(message, "Test message");
}

END_TEST

START_TEST(test_parse_error_null_code_val) {
    /* Line 364 branch: code_val is NULL (missing code field) */
    const char *json = "{"
                       "\"error\":{"
                       "\"type\":\"test_type\","
                       "\"message\":\"Test message\""
                       "}"
                       "}";

    ik_error_category_t category;
    char *message = NULL;
    res_t result = ik_openai_parse_error(test_ctx, 500, json, strlen(json),
                                         &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
    /* Should use type: message format when code is missing */
    ck_assert_str_eq(message, "test_type: Test message");
}

END_TEST

START_TEST(test_parse_error_null_msg_val) {
    /* Line 367 branch: msg_val is NULL (missing message field) */
    const char *json = "{"
                       "\"error\":{"
                       "\"type\":\"test_type\","
                       "\"code\":\"test_code\""
                       "}"
                       "}";

    ik_error_category_t category;
    char *message = NULL;
    res_t result = ik_openai_parse_error(test_ctx, 500, json, strlen(json),
                                         &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
    /* Should use type only when message is missing */
    ck_assert_str_eq(message, "test_type");
}

END_TEST

START_TEST(test_parse_multiple_tool_calls_second_invalid) {
    /* Line 295 branches: foreach loop with error on second iteration */
    /* This tests the error return path inside the tool_calls foreach loop */
    const char *json = "{"
                       "\"id\":\"chatcmpl-test\","
                       "\"model\":\"gpt-4\","
                       "\"choices\":[{"
                       "\"index\":0,"
                       "\"message\":{"
                       "\"role\":\"assistant\","
                       "\"content\":null,"
                       "\"tool_calls\":["
                       "{"
                       "\"id\":\"call_1\","
                       "\"type\":\"function\","
                       "\"function\":{"
                       "\"name\":\"func1\","
                       "\"arguments\":\"{}\""
                       "}"
                       "},"
                       "{"
                       "\"id\":\"call_2\","
                       "\"type\":\"function\","
                       "\"function\":{"
                       "\"arguments\":\"{}\""
                       "}"
                       "}"
                       "]"
                       "},"
                       "\"finish_reason\":\"tool_calls\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_PARSE);
    /* Second tool call is missing 'name', should trigger error in foreach loop */
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *response_chat_coverage_branches_suite(void)
{
    Suite *s = suite_create("OpenAI Chat Response - Branch Coverage");

    TCase *tc_branches = tcase_create("Missing Branch Coverage");
    tcase_set_timeout(tc_branches, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_branches, setup, teardown);
    tcase_add_test(tc_branches, test_parse_error_empty_json_len);
    tcase_add_test(tc_branches, test_parse_error_null_type_val);
    tcase_add_test(tc_branches, test_parse_error_null_code_val);
    tcase_add_test(tc_branches, test_parse_error_null_msg_val);
    tcase_add_test(tc_branches, test_parse_multiple_tool_calls_second_invalid);
    suite_add_tcase(s, tc_branches);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = response_chat_coverage_branches_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/openai/response_chat_coverage_branches_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
