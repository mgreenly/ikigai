/**
 * @file response_usage_coverage_test.c
 * @brief Coverage tests for Google response.c usage metadata edge cases
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/google/response.h"
#include "providers/provider.h"

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
 * Usage Metadata Field Combinations
 * ================================================================ */

START_TEST(test_parse_usage_missing_prompt_tokens) {
    const char *json = "{"
                       "\"modelVersion\":\"gemini-2.5-flash\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":[{\"text\":\"Hello\"}]},"
                       "\"finishReason\":\"STOP\""
                       "}],"
                       "\"usageMetadata\":{"
                       "\"candidatesTokenCount\":20,"
                       "\"thoughtsTokenCount\":5,"
                       "\"totalTokenCount\":25"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->usage.input_tokens, 0);
    ck_assert_int_eq(resp->usage.thinking_tokens, 5);
    ck_assert_int_eq(resp->usage.output_tokens, 15);
    ck_assert_int_eq(resp->usage.total_tokens, 25);
}
END_TEST

START_TEST(test_parse_usage_missing_candidates_tokens) {
    const char *json = "{"
                       "\"modelVersion\":\"gemini-2.5-flash\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":[{\"text\":\"Hello\"}]},"
                       "\"finishReason\":\"STOP\""
                       "}],"
                       "\"usageMetadata\":{"
                       "\"promptTokenCount\":10,"
                       "\"thoughtsTokenCount\":5,"
                       "\"totalTokenCount\":15"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->usage.input_tokens, 10);
    ck_assert_int_eq(resp->usage.thinking_tokens, 5);
    ck_assert_int_eq(resp->usage.output_tokens, -5);
    ck_assert_int_eq(resp->usage.total_tokens, 15);
}
END_TEST

START_TEST(test_parse_usage_missing_thoughts_tokens) {
    const char *json = "{"
                       "\"modelVersion\":\"gemini-2.5-flash\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":[{\"text\":\"Hello\"}]},"
                       "\"finishReason\":\"STOP\""
                       "}],"
                       "\"usageMetadata\":{"
                       "\"promptTokenCount\":10,"
                       "\"candidatesTokenCount\":15,"
                       "\"totalTokenCount\":25"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->usage.input_tokens, 10);
    ck_assert_int_eq(resp->usage.thinking_tokens, 0);
    ck_assert_int_eq(resp->usage.output_tokens, 15);
    ck_assert_int_eq(resp->usage.total_tokens, 25);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_response_usage_coverage_suite(void)
{
    Suite *s = suite_create("Google Response Usage Coverage");

    TCase *tc_usage = tcase_create("Usage metadata field combinations");
    tcase_set_timeout(tc_usage, 30);
    tcase_add_unchecked_fixture(tc_usage, setup, teardown);
    tcase_add_test(tc_usage, test_parse_usage_missing_prompt_tokens);
    tcase_add_test(tc_usage, test_parse_usage_missing_candidates_tokens);
    tcase_add_test(tc_usage, test_parse_usage_missing_thoughts_tokens);
    suite_add_tcase(s, tc_usage);

    return s;
}

int main(void)
{
    Suite *s = google_response_usage_coverage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
