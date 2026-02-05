#include "tests/test_constants.h"
/**
 * @file response_responses_usage_test.c
 * @brief Tests for OpenAI Responses API usage parsing coverage
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
 * Coverage Tests for parse_usage
 * ================================================================ */

START_TEST(test_parse_usage_prompt_tokens_not_int) {
    const char *json = "{"
                       "\"id\":\"resp-usage\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[],"
                       "\"usage\":{"
                       "\"prompt_tokens\":\"not an int\","
                       "\"completion_tokens\":10,"
                       "\"total_tokens\":15"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->usage.input_tokens, 0);
    ck_assert_int_eq(resp->usage.output_tokens, 10);
    ck_assert_int_eq(resp->usage.total_tokens, 15);
}

END_TEST

START_TEST(test_parse_usage_completion_tokens_not_int) {
    const char *json = "{"
                       "\"id\":\"resp-usage\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":\"not an int\","
                       "\"total_tokens\":15"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->usage.input_tokens, 5);
    ck_assert_int_eq(resp->usage.output_tokens, 0);
    ck_assert_int_eq(resp->usage.total_tokens, 15);
}

END_TEST

START_TEST(test_parse_usage_total_tokens_not_int) {
    const char *json = "{"
                       "\"id\":\"resp-usage\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":10,"
                       "\"total_tokens\":\"not an int\""
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->usage.input_tokens, 5);
    ck_assert_int_eq(resp->usage.output_tokens, 10);
    ck_assert_int_eq(resp->usage.total_tokens, 0);
}

END_TEST

START_TEST(test_parse_usage_reasoning_tokens_not_int) {
    const char *json = "{"
                       "\"id\":\"resp-usage\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":10,"
                       "\"total_tokens\":15,"
                       "\"completion_tokens_details\":{"
                       "\"reasoning_tokens\":\"not an int\""
                       "}"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->usage.thinking_tokens, 0);
}

END_TEST

START_TEST(test_parse_usage_tokens_null) {
    const char *json = "{"
                       "\"id\":\"resp-usage\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[],"
                       "\"usage\":{"
                       "\"prompt_tokens\":null,"
                       "\"completion_tokens\":null,"
                       "\"total_tokens\":null"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->usage.input_tokens, 0);
    ck_assert_int_eq(resp->usage.output_tokens, 0);
    ck_assert_int_eq(resp->usage.total_tokens, 0);
}

END_TEST

START_TEST(test_parse_usage_reasoning_tokens_null) {
    const char *json = "{"
                       "\"id\":\"resp-usage\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":10,"
                       "\"total_tokens\":15,"
                       "\"completion_tokens_details\":{"
                       "\"reasoning_tokens\":null"
                       "}"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->usage.thinking_tokens, 0);
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *response_responses_usage_suite(void)
{
    Suite *s = suite_create("OpenAI Responses API Usage Parsing Tests");

    TCase *tc_usage = tcase_create("Usage Parsing Coverage");
    tcase_set_timeout(tc_usage, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_usage, setup, teardown);
    tcase_add_test(tc_usage, test_parse_usage_prompt_tokens_not_int);
    tcase_add_test(tc_usage, test_parse_usage_completion_tokens_not_int);
    tcase_add_test(tc_usage, test_parse_usage_total_tokens_not_int);
    tcase_add_test(tc_usage, test_parse_usage_reasoning_tokens_not_int);
    tcase_add_test(tc_usage, test_parse_usage_tokens_null);
    tcase_add_test(tc_usage, test_parse_usage_reasoning_tokens_null);
    suite_add_tcase(s, tc_usage);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = response_responses_usage_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/openai/response_responses_usage_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
