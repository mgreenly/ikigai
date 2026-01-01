/**
 * @file response_chat_tool_errors_test.c
 * @brief Coverage tests for OpenAI Chat tool call error paths
 *
 * Tests edge cases and error paths in tool call parsing.
 */

#include "../../../../src/providers/openai/response.h"
#include "../../../../src/providers/provider.h"
#include "../../../../src/error.h"

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
 * Tool Call Error Tests (parse_chat_tool_call branches)
 * ================================================================ */

START_TEST(test_parse_tool_call_missing_id) {
    /* Tool call without 'id' field - line 32 branch (id_val == NULL) */
    const char *json = "{"
                       "\"id\":\"chatcmpl-test\","
                       "\"model\":\"gpt-4\","
                       "\"choices\":[{"
                       "\"index\":0,"
                       "\"message\":{"
                       "\"role\":\"assistant\","
                       "\"content\":null,"
                       "\"tool_calls\":[{"
                       "\"type\":\"function\","
                       "\"function\":{"
                       "\"name\":\"test\","
                       "\"arguments\":\"{}\""
                       "}"
                       "}]"
                       "},"
                       "\"finish_reason\":\"tool_calls\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_PARSE);
    ck_assert_str_eq(result.err->msg, "Tool call missing 'id' field");
}

END_TEST

START_TEST(test_parse_tool_call_id_not_string) {
    /* Tool call with non-string 'id' - line 36 branch (id == NULL after yyjson_get_str) */
    const char *json = "{"
                       "\"id\":\"chatcmpl-test\","
                       "\"model\":\"gpt-4\","
                       "\"choices\":[{"
                       "\"index\":0,"
                       "\"message\":{"
                       "\"role\":\"assistant\","
                       "\"content\":null,"
                       "\"tool_calls\":[{"
                       "\"id\":123,"
                       "\"type\":\"function\","
                       "\"function\":{"
                       "\"name\":\"test\","
                       "\"arguments\":\"{}\""
                       "}"
                       "}]"
                       "},"
                       "\"finish_reason\":\"tool_calls\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_PARSE);
    ck_assert_str_eq(result.err->msg, "Tool call 'id' is not a string");
}

END_TEST

START_TEST(test_parse_tool_call_missing_function) {
    /* Tool call without 'function' field - line 44 branch (func_val == NULL) */
    const char *json = "{"
                       "\"id\":\"chatcmpl-test\","
                       "\"model\":\"gpt-4\","
                       "\"choices\":[{"
                       "\"index\":0,"
                       "\"message\":{"
                       "\"role\":\"assistant\","
                       "\"content\":null,"
                       "\"tool_calls\":[{"
                       "\"id\":\"call_123\","
                       "\"type\":\"function\""
                       "}]"
                       "},"
                       "\"finish_reason\":\"tool_calls\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_PARSE);
    ck_assert_str_eq(result.err->msg, "Tool call missing 'function' field");
}

END_TEST

START_TEST(test_parse_tool_call_missing_name) {
    /* Tool call function without 'name' field - line 50 branch (name_val == NULL) */
    const char *json = "{"
                       "\"id\":\"chatcmpl-test\","
                       "\"model\":\"gpt-4\","
                       "\"choices\":[{"
                       "\"index\":0,"
                       "\"message\":{"
                       "\"role\":\"assistant\","
                       "\"content\":null,"
                       "\"tool_calls\":[{"
                       "\"id\":\"call_123\","
                       "\"type\":\"function\","
                       "\"function\":{"
                       "\"arguments\":\"{}\""
                       "}"
                       "}]"
                       "},"
                       "\"finish_reason\":\"tool_calls\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_PARSE);
    ck_assert_str_eq(result.err->msg, "Tool call function missing 'name' field");
}

END_TEST

START_TEST(test_parse_tool_call_name_not_string) {
    /* Tool call function with non-string 'name' - line 54 branch (name == NULL) */
    const char *json = "{"
                       "\"id\":\"chatcmpl-test\","
                       "\"model\":\"gpt-4\","
                       "\"choices\":[{"
                       "\"index\":0,"
                       "\"message\":{"
                       "\"role\":\"assistant\","
                       "\"content\":null,"
                       "\"tool_calls\":[{"
                       "\"id\":\"call_123\","
                       "\"type\":\"function\","
                       "\"function\":{"
                       "\"name\":456,"
                       "\"arguments\":\"{}\""
                       "}"
                       "}]"
                       "},"
                       "\"finish_reason\":\"tool_calls\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_PARSE);
    ck_assert_str_eq(result.err->msg, "Tool call function 'name' is not a string");
}

END_TEST

START_TEST(test_parse_tool_call_missing_arguments) {
    /* Tool call function without 'arguments' field - line 62 branch (args_val == NULL) */
    const char *json = "{"
                       "\"id\":\"chatcmpl-test\","
                       "\"model\":\"gpt-4\","
                       "\"choices\":[{"
                       "\"index\":0,"
                       "\"message\":{"
                       "\"role\":\"assistant\","
                       "\"content\":null,"
                       "\"tool_calls\":[{"
                       "\"id\":\"call_123\","
                       "\"type\":\"function\","
                       "\"function\":{"
                       "\"name\":\"test_func\""
                       "}"
                       "}]"
                       "},"
                       "\"finish_reason\":\"tool_calls\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_PARSE);
    ck_assert_str_eq(result.err->msg, "Tool call function missing 'arguments' field");
}

END_TEST

START_TEST(test_parse_tool_call_arguments_not_string) {
    /* Tool call function with non-string 'arguments' - line 66 branch (args_str == NULL) */
    const char *json = "{"
                       "\"id\":\"chatcmpl-test\","
                       "\"model\":\"gpt-4\","
                       "\"choices\":[{"
                       "\"index\":0,"
                       "\"message\":{"
                       "\"role\":\"assistant\","
                       "\"content\":null,"
                       "\"tool_calls\":[{"
                       "\"id\":\"call_123\","
                       "\"type\":\"function\","
                       "\"function\":{"
                       "\"name\":\"test_func\","
                       "\"arguments\":789"
                       "}"
                       "}]"
                       "},"
                       "\"finish_reason\":\"tool_calls\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_PARSE);
    ck_assert_str_eq(result.err->msg, "Tool call function 'arguments' is not a string");
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *response_chat_tool_errors_suite(void)
{
    Suite *s = suite_create("OpenAI Chat Response Parsing - Tool Call Errors");

    TCase *tc_tool_errors = tcase_create("Tool Call Error Paths");
    tcase_set_timeout(tc_tool_errors, 30);
    tcase_add_checked_fixture(tc_tool_errors, setup, teardown);
    tcase_add_test(tc_tool_errors, test_parse_tool_call_missing_id);
    tcase_add_test(tc_tool_errors, test_parse_tool_call_id_not_string);
    tcase_add_test(tc_tool_errors, test_parse_tool_call_missing_function);
    tcase_add_test(tc_tool_errors, test_parse_tool_call_missing_name);
    tcase_add_test(tc_tool_errors, test_parse_tool_call_name_not_string);
    tcase_add_test(tc_tool_errors, test_parse_tool_call_missing_arguments);
    tcase_add_test(tc_tool_errors, test_parse_tool_call_arguments_not_string);
    suite_add_tcase(s, tc_tool_errors);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = response_chat_tool_errors_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
