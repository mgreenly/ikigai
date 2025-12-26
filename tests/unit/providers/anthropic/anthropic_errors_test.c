/**
 * @file test_anthropic_errors.c
 * @brief Unit tests for Anthropic error handling and HTTP status mapping
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/anthropic/error.h"
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
 * Error Handling Tests
 * ================================================================ */

START_TEST(test_handle_error_401_auth) {
    const char *error_json =
        "{"
        "  \"type\": \"error\","
        "  \"error\": {"
        "    \"type\": \"authentication_error\","
        "    \"message\": \"invalid x-api-key\""
        "  }"
        "}";

    ik_error_category_t category;
    res_t r = ik_anthropic_handle_error(test_ctx, 401, error_json, &category);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_AUTH);
}

END_TEST START_TEST(test_handle_error_403_auth)
{
    const char *error_json =
        "{"
        "  \"type\": \"error\","
        "  \"error\": {"
        "    \"type\": \"permission_error\","
        "    \"message\": \"Access denied\""
        "  }"
        "}";

    ik_error_category_t category;
    res_t r = ik_anthropic_handle_error(test_ctx, 403, error_json, &category);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_AUTH);
}

END_TEST START_TEST(test_handle_error_429_rate_limit)
{
    const char *error_json =
        "{"
        "  \"type\": \"error\","
        "  \"error\": {"
        "    \"type\": \"rate_limit_error\","
        "    \"message\": \"Rate limit exceeded\""
        "  }"
        "}";

    ik_error_category_t category;
    res_t r = ik_anthropic_handle_error(test_ctx, 429, error_json, &category);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_RATE_LIMIT);
}

END_TEST START_TEST(test_handle_error_400_invalid_arg)
{
    const char *error_json =
        "{"
        "  \"type\": \"error\","
        "  \"error\": {"
        "    \"type\": \"invalid_request_error\","
        "    \"message\": \"Invalid model specified\""
        "  }"
        "}";

    ik_error_category_t category;
    res_t r = ik_anthropic_handle_error(test_ctx, 400, error_json, &category);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_INVALID_ARG);
}

END_TEST START_TEST(test_handle_error_404_not_found)
{
    const char *error_json =
        "{"
        "  \"type\": \"error\","
        "  \"error\": {"
        "    \"type\": \"not_found_error\","
        "    \"message\": \"Resource not found\""
        "  }"
        "}";

    ik_error_category_t category;
    res_t r = ik_anthropic_handle_error(test_ctx, 404, error_json, &category);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_NOT_FOUND);
}

END_TEST START_TEST(test_handle_error_500_server)
{
    const char *error_json =
        "{"
        "  \"type\": \"error\","
        "  \"error\": {"
        "    \"type\": \"internal_server_error\","
        "    \"message\": \"Internal server error\""
        "  }"
        "}";

    ik_error_category_t category;
    res_t r = ik_anthropic_handle_error(test_ctx, 500, error_json, &category);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}

END_TEST START_TEST(test_handle_error_529_overloaded)
{
    const char *error_json =
        "{"
        "  \"type\": \"error\","
        "  \"error\": {"
        "    \"type\": \"overloaded_error\","
        "    \"message\": \"Service is temporarily overloaded\""
        "  }"
        "}";

    ik_error_category_t category;
    res_t r = ik_anthropic_handle_error(test_ctx, 529, error_json, &category);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}

END_TEST START_TEST(test_handle_error_unknown_status)
{
    const char *error_json =
        "{"
        "  \"type\": \"error\","
        "  \"error\": {"
        "    \"type\": \"unknown_error\","
        "    \"message\": \"Something went wrong\""
        "  }"
        "}";

    ik_error_category_t category;
    res_t r = ik_anthropic_handle_error(test_ctx, 418, error_json, &category);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_UNKNOWN);
}

END_TEST START_TEST(test_handle_error_invalid_json)
{
    const char *error_json = "not valid json";

    ik_error_category_t category;
    res_t r = ik_anthropic_handle_error(test_ctx, 500, error_json, &category);

    ck_assert(is_err(&r));
}

END_TEST START_TEST(test_handle_error_no_root)
{
    const char *error_json = "";

    ik_error_category_t category;
    res_t r = ik_anthropic_handle_error(test_ctx, 500, error_json, &category);

    ck_assert(is_err(&r));
}

END_TEST START_TEST(test_handle_error_with_error_object)
{
    const char *error_json =
        "{"
        "  \"type\": \"error\","
        "  \"error\": {"
        "    \"type\": \"rate_limit_error\","
        "    \"message\": \"Rate limit exceeded\""
        "  }"
        "}";

    ik_error_category_t category;
    res_t r = ik_anthropic_handle_error(test_ctx, 429, error_json, &category);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_RATE_LIMIT);
}

END_TEST START_TEST(test_handle_error_without_error_object)
{
    const char *error_json =
        "{"
        "  \"type\": \"error\""
        "}";

    ik_error_category_t category;
    res_t r = ik_anthropic_handle_error(test_ctx, 500, error_json, &category);

    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}

END_TEST
/* ================================================================
 * Retry-After Header Tests
 * ================================================================ */

START_TEST(test_retry_after_found)
{
    const char *headers[] = {
        "content-type: application/json",
        "retry-after: 60",
        "anthropic-ratelimit-requests-remaining: 0",
        NULL
    };

    int32_t retry_after = ik_anthropic_get_retry_after(headers);
    ck_assert_int_eq(retry_after, 60);
}

END_TEST START_TEST(test_retry_after_missing)
{
    const char *headers[] = {
        "content-type: application/json",
        "anthropic-ratelimit-requests-remaining: 0",
        NULL
    };

    int32_t retry_after = ik_anthropic_get_retry_after(headers);
    ck_assert_int_eq(retry_after, -1);
}

END_TEST START_TEST(test_retry_after_null_headers)
{
    int32_t retry_after = ik_anthropic_get_retry_after(NULL);
    ck_assert_int_eq(retry_after, -1);
}

END_TEST START_TEST(test_retry_after_case_insensitive)
{
    const char *headers[] = {
        "Retry-After: 120",
        "RETRY-AFTER: 240",
        NULL
    };

    int32_t retry_after = ik_anthropic_get_retry_after(headers);
    ck_assert_int_eq(retry_after, 120);
}

END_TEST START_TEST(test_retry_after_with_whitespace)
{
    const char *headers[] = {
        "retry-after:   \t  300",
        NULL
    };

    int32_t retry_after = ik_anthropic_get_retry_after(headers);
    ck_assert_int_eq(retry_after, 300);
}

END_TEST START_TEST(test_retry_after_invalid_value)
{
    const char *headers[] = {
        "retry-after: not-a-number",
        NULL
    };

    int32_t retry_after = ik_anthropic_get_retry_after(headers);
    ck_assert_int_eq(retry_after, -1);
}

END_TEST START_TEST(test_retry_after_negative_value)
{
    const char *headers[] = {
        "retry-after: -5",
        NULL
    };

    int32_t retry_after = ik_anthropic_get_retry_after(headers);
    ck_assert_int_eq(retry_after, -1);
}

END_TEST START_TEST(test_retry_after_zero_value)
{
    const char *headers[] = {
        "retry-after: 0",
        NULL
    };

    int32_t retry_after = ik_anthropic_get_retry_after(headers);
    ck_assert_int_eq(retry_after, -1);
}

END_TEST START_TEST(test_retry_after_empty_value)
{
    const char *headers[] = {
        "retry-after: ",
        NULL
    };

    int32_t retry_after = ik_anthropic_get_retry_after(headers);
    ck_assert_int_eq(retry_after, -1);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *anthropic_errors_suite(void)
{
    Suite *s = suite_create("Anthropic Errors");

    TCase *tc_errors = tcase_create("Error Handling");
    tcase_add_unchecked_fixture(tc_errors, setup, teardown);
    tcase_add_test(tc_errors, test_handle_error_401_auth);
    tcase_add_test(tc_errors, test_handle_error_403_auth);
    tcase_add_test(tc_errors, test_handle_error_429_rate_limit);
    tcase_add_test(tc_errors, test_handle_error_400_invalid_arg);
    tcase_add_test(tc_errors, test_handle_error_404_not_found);
    tcase_add_test(tc_errors, test_handle_error_500_server);
    tcase_add_test(tc_errors, test_handle_error_529_overloaded);
    tcase_add_test(tc_errors, test_handle_error_unknown_status);
    tcase_add_test(tc_errors, test_handle_error_invalid_json);
    tcase_add_test(tc_errors, test_handle_error_no_root);
    tcase_add_test(tc_errors, test_handle_error_with_error_object);
    tcase_add_test(tc_errors, test_handle_error_without_error_object);
    suite_add_tcase(s, tc_errors);

    TCase *tc_retry = tcase_create("Retry-After Headers");
    tcase_add_unchecked_fixture(tc_retry, setup, teardown);
    tcase_add_test(tc_retry, test_retry_after_found);
    tcase_add_test(tc_retry, test_retry_after_missing);
    tcase_add_test(tc_retry, test_retry_after_null_headers);
    tcase_add_test(tc_retry, test_retry_after_case_insensitive);
    tcase_add_test(tc_retry, test_retry_after_with_whitespace);
    tcase_add_test(tc_retry, test_retry_after_invalid_value);
    tcase_add_test(tc_retry, test_retry_after_negative_value);
    tcase_add_test(tc_retry, test_retry_after_zero_value);
    tcase_add_test(tc_retry, test_retry_after_empty_value);
    suite_add_tcase(s, tc_retry);

    return s;
}

int main(void)
{
    Suite *s = anthropic_errors_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
