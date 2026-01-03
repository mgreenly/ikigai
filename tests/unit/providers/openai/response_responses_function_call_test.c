/**
 * @file response_responses_function_call_test.c
 * @brief Tests for OpenAI Responses API function call coverage
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
 * Coverage Tests for parse_function_call
 * ================================================================ */

START_TEST(test_parse_function_call_id_null) {
    const char *json = "{"
                       "\"id\":\"resp-func\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"function_call\","
                       "\"id\":null,"
                       "\"name\":\"test_func\","
                       "\"arguments\":\"{}\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
}

END_TEST

START_TEST(test_parse_function_call_name_null) {
    const char *json = "{"
                       "\"id\":\"resp-func\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"function_call\","
                       "\"id\":\"test-id\","
                       "\"name\":null,"
                       "\"arguments\":\"{}\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
}

END_TEST

START_TEST(test_parse_function_call_arguments_null) {
    const char *json = "{"
                       "\"id\":\"resp-func\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"function_call\","
                       "\"id\":\"test-id\","
                       "\"name\":\"test_func\","
                       "\"arguments\":null"
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
}

END_TEST

START_TEST(test_parse_function_call_missing_name) {
    const char *json = "{"
                       "\"id\":\"resp-func\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"function_call\","
                       "\"id\":\"test-id\","
                       "\"arguments\":\"{}\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
}

END_TEST

START_TEST(test_parse_function_call_missing_arguments) {
    const char *json = "{"
                       "\"id\":\"resp-func\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"function_call\","
                       "\"id\":\"test-id\","
                       "\"name\":\"test_func\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
}

END_TEST

START_TEST(test_parse_function_call_with_call_id) {
    const char *json = "{"
                       "\"id\":\"resp-func\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"function_call\","
                       "\"id\":\"old-id\","
                       "\"call_id\":\"new-id\","
                       "\"name\":\"test_func\","
                       "\"arguments\":\"{}\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 1);
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.id, "new-id");
}

END_TEST

START_TEST(test_parse_function_call_call_id_null) {
    const char *json = "{"
                       "\"id\":\"resp-func\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"function_call\","
                       "\"id\":\"fallback-id\","
                       "\"call_id\":null,"
                       "\"name\":\"test_func\","
                       "\"arguments\":\"{}\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 1);
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.id, "fallback-id");
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *response_responses_function_call_suite(void)
{
    Suite *s = suite_create("OpenAI Responses API Function Call Tests");

    TCase *tc_function = tcase_create("Function Call Coverage");
    tcase_set_timeout(tc_function, 30);
    tcase_add_checked_fixture(tc_function, setup, teardown);
    tcase_add_test(tc_function, test_parse_function_call_id_null);
    tcase_add_test(tc_function, test_parse_function_call_name_null);
    tcase_add_test(tc_function, test_parse_function_call_arguments_null);
    tcase_add_test(tc_function, test_parse_function_call_missing_name);
    tcase_add_test(tc_function, test_parse_function_call_missing_arguments);
    tcase_add_test(tc_function, test_parse_function_call_with_call_id);
    tcase_add_test(tc_function, test_parse_function_call_call_id_null);
    suite_add_tcase(s, tc_function);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = response_responses_function_call_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
