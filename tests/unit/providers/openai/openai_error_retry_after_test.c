/**
 * @file openai_error_retry_after_test.c
 * @brief Unit tests for ik_openai_get_retry_after function
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/openai/error.h"

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
 * ik_openai_get_retry_after Tests
 * ================================================================ */

START_TEST(test_retry_after_null_headers) {
    int32_t retry_after = ik_openai_get_retry_after(NULL);
    ck_assert_int_eq(retry_after, -1);
}
END_TEST

START_TEST(test_retry_after_both_headers_prefer_minimum) {
    const char *headers[] = {
        "content-type: application/json",
        "x-ratelimit-reset-requests: 30s",
        "x-ratelimit-reset-tokens: 60s",
        NULL
    };

    int32_t retry_after = ik_openai_get_retry_after(headers);
    ck_assert_int_eq(retry_after, 30);
}

END_TEST

START_TEST(test_retry_after_both_headers_prefer_tokens) {
    const char *headers[] = {
        "content-type: application/json",
        "x-ratelimit-reset-requests: 60s",
        "x-ratelimit-reset-tokens: 30s",
        NULL
    };

    int32_t retry_after = ik_openai_get_retry_after(headers);
    ck_assert_int_eq(retry_after, 30);
}

END_TEST

START_TEST(test_retry_after_hours) {
    const char *headers[] = {
        "x-ratelimit-reset-requests: 1h",
        NULL
    };

    int32_t retry_after = ik_openai_get_retry_after(headers);
    ck_assert_int_eq(retry_after, 3600);
}

END_TEST

START_TEST(test_retry_after_complex_duration) {
    const char *headers[] = {
        "x-ratelimit-reset-requests: 1h30m45s",
        NULL
    };

    int32_t retry_after = ik_openai_get_retry_after(headers);
    ck_assert_int_eq(retry_after, 3600 + 1800 + 45);  // 5445 seconds
}

END_TEST

START_TEST(test_retry_after_invalid_duration) {
    const char *headers[] = {
        "x-ratelimit-reset-requests: invalid",
        NULL
    };

    int32_t retry_after = ik_openai_get_retry_after(headers);
    ck_assert_int_eq(retry_after, -1);
}

END_TEST

START_TEST(test_retry_after_unknown_unit) {
    const char *headers[] = {
        "x-ratelimit-reset-requests: 30x",
        NULL
    };

    int32_t retry_after = ik_openai_get_retry_after(headers);
    ck_assert_int_eq(retry_after, -1);
}

END_TEST

START_TEST(test_retry_after_whitespace) {
    const char *headers[] = {
        "x-ratelimit-reset-requests:   \t  30s",
        NULL
    };

    int32_t retry_after = ik_openai_get_retry_after(headers);
    ck_assert_int_eq(retry_after, 30);
}

END_TEST

START_TEST(test_retry_after_case_insensitive) {
    const char *headers[] = {
        "X-RateLimit-Reset-Requests: 30s",
        NULL
    };

    int32_t retry_after = ik_openai_get_retry_after(headers);
    ck_assert_int_eq(retry_after, 30);
}

END_TEST

START_TEST(test_retry_after_tokens_case_insensitive) {
    const char *headers[] = {
        "X-RateLimit-Reset-Tokens: 60s",
        NULL
    };

    int32_t retry_after = ik_openai_get_retry_after(headers);
    ck_assert_int_eq(retry_after, 60);
}

END_TEST

START_TEST(test_retry_after_empty_value) {
    const char *headers[] = {
        "x-ratelimit-reset-requests: ",
        NULL
    };

    int32_t retry_after = ik_openai_get_retry_after(headers);
    // Empty string after colon and whitespace returns 0 (edge case)
    ck_assert_int_eq(retry_after, 0);
}

END_TEST

START_TEST(test_retry_after_only_whitespace) {
    const char *headers[] = {
        "x-ratelimit-reset-requests:    \t  ",
        NULL
    };

    int32_t retry_after = ik_openai_get_retry_after(headers);
    // Whitespace gets skipped, empty string returns 0 (edge case)
    ck_assert_int_eq(retry_after, 0);
}

END_TEST

START_TEST(test_retry_after_multiple_same_headers) {
    const char *headers[] = {
        "x-ratelimit-reset-requests: 60s",
        "x-ratelimit-reset-requests: 30s",
        NULL
    };

    // Should use the last one
    int32_t retry_after = ik_openai_get_retry_after(headers);
    ck_assert_int_eq(retry_after, 30);
}

END_TEST

START_TEST(test_retry_after_zero_duration) {
    const char *headers[] = {
        "x-ratelimit-reset-requests: 0s",
        NULL
    };

    int32_t retry_after = ik_openai_get_retry_after(headers);
    ck_assert_int_eq(retry_after, 0);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *openai_error_retry_after_suite(void)
{
    Suite *s = suite_create("OpenAI Error Retry After");

    TCase *tc_retry = tcase_create("Retry After");
    tcase_set_timeout(tc_retry, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_retry, setup, teardown);
    tcase_add_test(tc_retry, test_retry_after_null_headers);
    tcase_add_test(tc_retry, test_retry_after_both_headers_prefer_minimum);
    tcase_add_test(tc_retry, test_retry_after_both_headers_prefer_tokens);
    tcase_add_test(tc_retry, test_retry_after_hours);
    tcase_add_test(tc_retry, test_retry_after_complex_duration);
    tcase_add_test(tc_retry, test_retry_after_invalid_duration);
    tcase_add_test(tc_retry, test_retry_after_unknown_unit);
    tcase_add_test(tc_retry, test_retry_after_whitespace);
    tcase_add_test(tc_retry, test_retry_after_case_insensitive);
    tcase_add_test(tc_retry, test_retry_after_tokens_case_insensitive);
    tcase_add_test(tc_retry, test_retry_after_empty_value);
    tcase_add_test(tc_retry, test_retry_after_only_whitespace);
    tcase_add_test(tc_retry, test_retry_after_multiple_same_headers);
    tcase_add_test(tc_retry, test_retry_after_zero_duration);
    suite_add_tcase(s, tc_retry);

    return s;
}

int main(void)
{
    Suite *s = openai_error_retry_after_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
