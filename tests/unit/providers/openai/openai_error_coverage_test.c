/**
 * @file openai_error_coverage_test.c
 * @brief Additional coverage tests for OpenAI error handling edge cases
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
 * Coverage gap tests for ik_openai_handle_error
 * ================================================================ */

/**
 * Test: Missing 'code' field in error object
 * Covers line 75: yyjson_obj_get returns NULL when field doesn't exist
 */
START_TEST(test_handle_error_missing_code_field)
{
    // JSON with error object but no 'code' field
    const char *json = "{\"error\": {\"message\": \"Error\", \"type\": \"test_type\"}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 500, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}
END_TEST

/**
 * Test: Missing 'type' field in error object
 * Covers line 76: yyjson_obj_get returns NULL when field doesn't exist
 */
START_TEST(test_handle_error_missing_type_field)
{
    // JSON with error object but no 'type' field
    const char *json = "{\"error\": {\"message\": \"Error\", \"code\": \"test_code\"}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 500, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}
END_TEST

/**
 * Test: Both 'code' and 'type' fields missing
 * Covers both lines 75 and 76 returning NULL
 */
START_TEST(test_handle_error_missing_code_and_type)
{
    // JSON with error object but neither 'code' nor 'type'
    const char *json = "{\"error\": {\"message\": \"Error message only\"}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 400, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_INVALID_ARG);
}
END_TEST

/**
 * Test: 'code' field present and is a string (false branch of ternary)
 * Covers line 78: yyjson_is_str(code_val) ? ... : NULL (true branch)
 * The existing tests already cover when code is a string, but we need
 * to ensure the false branch is taken when code_val is NULL
 */
START_TEST(test_handle_error_code_null_ternary)
{
    // This is covered by missing_code_field test, but making explicit
    const char *json = "{\"error\": {\"type\": \"error\"}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 500, json, &category);
    ck_assert(!is_err(&r));
    // When code is NULL, code_val is NULL, so yyjson_is_str returns false
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}
END_TEST

/* ================================================================
 * Coverage gap tests for ik_openai_get_retry_after
 * ================================================================ */

/**
 * Test: Header value with no whitespace after colon
 * Covers line 172: while loop exits immediately (false branch first time)
 */
START_TEST(test_retry_after_no_whitespace)
{
    const char *headers[] = {
        "x-ratelimit-reset-requests:30s",  // No space after colon
        NULL
    };

    int32_t retry_after = ik_openai_get_retry_after(headers);
    ck_assert_int_eq(retry_after, 30);
}
END_TEST

/**
 * Test: Header value for tokens with no whitespace
 * Covers line 172 in the tokens branch
 */
START_TEST(test_retry_after_tokens_no_whitespace)
{
    const char *headers[] = {
        "x-ratelimit-reset-tokens:60s",  // No space after colon
        NULL
    };

    int32_t retry_after = ik_openai_get_retry_after(headers);
    ck_assert_int_eq(retry_after, 60);
}
END_TEST

/**
 * Test: Header value with tab character after colon
 * Covers line 172: branch for '\t' check
 */
START_TEST(test_retry_after_with_tab)
{
    const char *headers[] = {
        "x-ratelimit-reset-requests:\t30s",  // Tab after colon
        NULL
    };

    int32_t retry_after = ik_openai_get_retry_after(headers);
    ck_assert_int_eq(retry_after, 30);
}
END_TEST

/**
 * Test: Header value with tab character for tokens
 * Covers line 172: branch for '\t' check in tokens path
 */
START_TEST(test_retry_after_tokens_with_tab)
{
    const char *headers[] = {
        "x-ratelimit-reset-tokens:\t60s",  // Tab after colon
        NULL
    };

    int32_t retry_after = ik_openai_get_retry_after(headers);
    ck_assert_int_eq(retry_after, 60);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *openai_error_coverage_suite(void)
{
    Suite *s = suite_create("OpenAI Error Coverage");

    TCase *tc_handle = tcase_create("Handle Error Coverage");
    tcase_add_unchecked_fixture(tc_handle, setup, teardown);
    tcase_add_test(tc_handle, test_handle_error_missing_code_field);
    tcase_add_test(tc_handle, test_handle_error_missing_type_field);
    tcase_add_test(tc_handle, test_handle_error_missing_code_and_type);
    tcase_add_test(tc_handle, test_handle_error_code_null_ternary);
    suite_add_tcase(s, tc_handle);

    TCase *tc_retry = tcase_create("Retry After Coverage");
    tcase_add_unchecked_fixture(tc_retry, setup, teardown);
    tcase_add_test(tc_retry, test_retry_after_no_whitespace);
    tcase_add_test(tc_retry, test_retry_after_tokens_no_whitespace);
    tcase_add_test(tc_retry, test_retry_after_with_tab);
    tcase_add_test(tc_retry, test_retry_after_tokens_with_tab);
    suite_add_tcase(s, tc_retry);

    return s;
}

int main(void)
{
    Suite *s = openai_error_coverage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
