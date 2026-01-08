/**
 * @file response_chat_usage_test.c
 * @brief Coverage tests for OpenAI Chat usage parsing
 *
 * Tests edge cases in usage token parsing.
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
 * Usage Parsing Tests
 * ================================================================ */

START_TEST(test_parse_usage_null_prompt_tokens) {
    /* Usage with missing prompt_tokens - line 93 branch (prompt_val == NULL) */
    const char *json = "{"
                       "\"id\":\"chatcmpl-test\","
                       "\"model\":\"gpt-4\","
                       "\"choices\":[{"
                       "\"index\":0,"
                       "\"message\":{"
                       "\"role\":\"assistant\","
                       "\"content\":\"Test\""
                       "},"
                       "\"finish_reason\":\"stop\""
                       "}],"
                       "\"usage\":{"
                       "\"completion_tokens\":10,"
                       "\"total_tokens\":10"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    /* prompt_tokens should be 0 when missing */
    ck_assert_int_eq(resp->usage.input_tokens, 0);
    ck_assert_int_eq(resp->usage.output_tokens, 10);
}

END_TEST

START_TEST(test_parse_usage_non_int_prompt_tokens) {
    /* Usage with non-integer prompt_tokens - line 93 branch (!yyjson_is_int) */
    const char *json = "{"
                       "\"id\":\"chatcmpl-test\","
                       "\"model\":\"gpt-4\","
                       "\"choices\":[{"
                       "\"index\":0,"
                       "\"message\":{"
                       "\"role\":\"assistant\","
                       "\"content\":\"Test\""
                       "},"
                       "\"finish_reason\":\"stop\""
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":\"not_a_number\","
                       "\"completion_tokens\":10,"
                       "\"total_tokens\":10"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    /* prompt_tokens should be 0 when not an integer */
    ck_assert_int_eq(resp->usage.input_tokens, 0);
    ck_assert_int_eq(resp->usage.output_tokens, 10);
}

END_TEST

START_TEST(test_parse_usage_non_int_completion_tokens) {
    /* Usage with non-integer completion_tokens - line 99 branch */
    const char *json = "{"
                       "\"id\":\"chatcmpl-test\","
                       "\"model\":\"gpt-4\","
                       "\"choices\":[{"
                       "\"index\":0,"
                       "\"message\":{"
                       "\"role\":\"assistant\","
                       "\"content\":\"Test\""
                       "},"
                       "\"finish_reason\":\"stop\""
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":\"not_an_int\","
                       "\"total_tokens\":5"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->usage.input_tokens, 5);
    /* completion_tokens should be 0 when not an integer */
    ck_assert_int_eq(resp->usage.output_tokens, 0);
}

END_TEST

START_TEST(test_parse_usage_null_completion_tokens) {
    /* Usage with missing completion_tokens - line 99 branch (completion_val == NULL) */
    const char *json = "{"
                       "\"id\":\"chatcmpl-test\","
                       "\"model\":\"gpt-4\","
                       "\"choices\":[{"
                       "\"index\":0,"
                       "\"message\":{"
                       "\"role\":\"assistant\","
                       "\"content\":\"Test\""
                       "},"
                       "\"finish_reason\":\"stop\""
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"total_tokens\":5"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->usage.input_tokens, 5);
    /* completion_tokens should be 0 when missing */
    ck_assert_int_eq(resp->usage.output_tokens, 0);
}

END_TEST

START_TEST(test_parse_usage_non_int_total_tokens) {
    /* Usage with non-integer total_tokens - line 105 branch */
    const char *json = "{"
                       "\"id\":\"chatcmpl-test\","
                       "\"model\":\"gpt-4\","
                       "\"choices\":[{"
                       "\"index\":0,"
                       "\"message\":{"
                       "\"role\":\"assistant\","
                       "\"content\":\"Test\""
                       "},"
                       "\"finish_reason\":\"stop\""
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":10,"
                       "\"total_tokens\":\"not_an_int\""
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->usage.input_tokens, 5);
    ck_assert_int_eq(resp->usage.output_tokens, 10);
    /* total_tokens should be 0 when not an integer */
    ck_assert_int_eq(resp->usage.total_tokens, 0);
}

END_TEST

START_TEST(test_parse_usage_null_total_tokens) {
    /* Usage with missing total_tokens - line 105 branch (total_val == NULL) */
    const char *json = "{"
                       "\"id\":\"chatcmpl-test\","
                       "\"model\":\"gpt-4\","
                       "\"choices\":[{"
                       "\"index\":0,"
                       "\"message\":{"
                       "\"role\":\"assistant\","
                       "\"content\":\"Test\""
                       "},"
                       "\"finish_reason\":\"stop\""
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":10"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->usage.input_tokens, 5);
    ck_assert_int_eq(resp->usage.output_tokens, 10);
    /* total_tokens should be 0 when missing */
    ck_assert_int_eq(resp->usage.total_tokens, 0);
}

END_TEST

START_TEST(test_parse_usage_non_int_reasoning_tokens) {
    /* Usage with non-integer reasoning_tokens - line 113 branch */
    const char *json = "{"
                       "\"id\":\"chatcmpl-test\","
                       "\"model\":\"gpt-4\","
                       "\"choices\":[{"
                       "\"index\":0,"
                       "\"message\":{"
                       "\"role\":\"assistant\","
                       "\"content\":\"Test\""
                       "},"
                       "\"finish_reason\":\"stop\""
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":10,"
                       "\"total_tokens\":15,"
                       "\"completion_tokens_details\":{"
                       "\"reasoning_tokens\":\"not_an_int\""
                       "}"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->usage.input_tokens, 5);
    ck_assert_int_eq(resp->usage.output_tokens, 10);
    /* reasoning_tokens should be 0 when not an integer */
    ck_assert_int_eq(resp->usage.thinking_tokens, 0);
}

END_TEST

START_TEST(test_parse_usage_null_reasoning_tokens) {
    /* Usage with missing reasoning_tokens - line 113 branch (reasoning_val == NULL) */
    const char *json = "{"
                       "\"id\":\"chatcmpl-test\","
                       "\"model\":\"gpt-4\","
                       "\"choices\":[{"
                       "\"index\":0,"
                       "\"message\":{"
                       "\"role\":\"assistant\","
                       "\"content\":\"Test\""
                       "},"
                       "\"finish_reason\":\"stop\""
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":10,"
                       "\"total_tokens\":15,"
                       "\"completion_tokens_details\":{}"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->usage.input_tokens, 5);
    ck_assert_int_eq(resp->usage.output_tokens, 10);
    /* reasoning_tokens should be 0 when missing */
    ck_assert_int_eq(resp->usage.thinking_tokens, 0);
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *response_chat_usage_suite(void)
{
    Suite *s = suite_create("OpenAI Chat Response Parsing - Usage");

    TCase *tc_usage = tcase_create("Usage Parsing");
    tcase_set_timeout(tc_usage, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_usage, setup, teardown);
    tcase_add_test(tc_usage, test_parse_usage_null_prompt_tokens);
    tcase_add_test(tc_usage, test_parse_usage_non_int_prompt_tokens);
    tcase_add_test(tc_usage, test_parse_usage_null_completion_tokens);
    tcase_add_test(tc_usage, test_parse_usage_non_int_completion_tokens);
    tcase_add_test(tc_usage, test_parse_usage_null_total_tokens);
    tcase_add_test(tc_usage, test_parse_usage_non_int_total_tokens);
    tcase_add_test(tc_usage, test_parse_usage_null_reasoning_tokens);
    tcase_add_test(tc_usage, test_parse_usage_non_int_reasoning_tokens);
    suite_add_tcase(s, tc_usage);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = response_chat_usage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
