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

/**
 * Test: Content filter detected via type when code doesn't match
 * Covers line 82: is_content_filter(type) when is_content_filter(code) is false
 */
START_TEST(test_handle_error_content_filter_type_only)
{
    // JSON with content_filter in type but not in code
    const char *json = "{\"error\": {\"message\": \"Filtered\", \"type\": \"content_filter\", \"code\": \"other_code\"}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 400, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_CONTENT_FILTER);
}
END_TEST

/**
 * Test: Code field present but doesn't match any specific error codes
 * Covers line 86: else if (code != NULL) path with no matches
 */
START_TEST(test_handle_error_code_no_match)
{
    // JSON with a code that doesn't match any specific patterns
    const char *json = "{\"error\": {\"message\": \"Error\", \"type\": \"error\", \"code\": \"unknown_error_code\"}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 500, json, &category);
    ck_assert(!is_err(&r));
    // Should fall back to status-based category
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}
END_TEST

/**
 * Test: Both code and type are NULL (neither field present)
 * Covers line 43-44: is_content_filter(NULL) returning false
 */
START_TEST(test_handle_error_both_null_for_content_filter)
{
    // JSON with error object but neither code nor type
    const char *json = "{\"error\": {\"message\": \"Error\"}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 400, json, &category);
    ck_assert(!is_err(&r));
    // Should use status-based category (400 -> INVALID_ARG)
    ck_assert_int_eq(category, IK_ERR_CAT_INVALID_ARG);
}
END_TEST

/**
 * Test: Both headers present, requests is valid but tokens is invalid
 * Covers line 180-186: reset_requests >= 0 && reset_tokens >= 0 is false
 */
START_TEST(test_retry_after_requests_valid_tokens_invalid)
{
    const char *headers[] = {
        "x-ratelimit-reset-requests: 30s",
        "x-ratelimit-reset-tokens: invalid",
        NULL
    };

    int32_t retry_after = ik_openai_get_retry_after(headers);
    // Only requests is valid, so return it (line 182-183)
    ck_assert_int_eq(retry_after, 30);
}
END_TEST

/**
 * Test: Both headers present, requests is invalid but tokens is valid
 * Covers line 184-185: reset_tokens >= 0 when reset_requests < 0
 */
START_TEST(test_retry_after_requests_invalid_tokens_valid)
{
    const char *headers[] = {
        "x-ratelimit-reset-requests: invalid",
        "x-ratelimit-reset-tokens: 60s",
        NULL
    };

    int32_t retry_after = ik_openai_get_retry_after(headers);
    // Only tokens is valid, so return it (line 184-185)
    ck_assert_int_eq(retry_after, 60);
}
END_TEST

/**
 * Test: Header with minutes unit in parse_duration
 * Covers line 128-129: unit == 'm' branch
 */
START_TEST(test_retry_after_minutes_unit)
{
    const char *headers[] = {
        "x-ratelimit-reset-requests: 5m",
        NULL
    };

    int32_t retry_after = ik_openai_get_retry_after(headers);
    ck_assert_int_eq(retry_after, 300);
}
END_TEST

/**
 * Test: Non-matching header that starts similarly
 * Covers the negative branch of strncasecmp comparisons
 */
START_TEST(test_retry_after_non_matching_header)
{
    const char *headers[] = {
        "x-ratelimit-reset: 30s",  // Missing -requests or -tokens suffix
        "x-ratelimit-reset-other: 45s",  // Different suffix
        NULL
    };

    int32_t retry_after = ik_openai_get_retry_after(headers);
    // No matching headers, should return -1
    ck_assert_int_eq(retry_after, -1);
}
END_TEST

/**
 * Test: Both headers with equal values
 * Covers line 181: ternary operator when reset_requests >= reset_tokens
 */
START_TEST(test_retry_after_equal_values)
{
    const char *headers[] = {
        "x-ratelimit-reset-requests: 30s",
        "x-ratelimit-reset-tokens: 30s",
        NULL
    };

    int32_t retry_after = ik_openai_get_retry_after(headers);
    // When equal, should return reset_tokens (the second operand of ternary)
    ck_assert_int_eq(retry_after, 30);
}
END_TEST

/**
 * Test: Requests header greater than tokens
 * Covers line 181: ternary operator false branch (reset_requests >= reset_tokens)
 */
START_TEST(test_retry_after_requests_greater_than_tokens)
{
    const char *headers[] = {
        "x-ratelimit-reset-requests: 90s",
        "x-ratelimit-reset-tokens: 30s",
        NULL
    };

    int32_t retry_after = ik_openai_get_retry_after(headers);
    // Should return the minimum (tokens)
    ck_assert_int_eq(retry_after, 30);
}
END_TEST

/**
 * Test: Duration string with number but no unit
 * Covers line 127-137: else branch when unit is not m, s, or h
 */
START_TEST(test_retry_after_number_no_unit)
{
    const char *headers[] = {
        "x-ratelimit-reset-requests: 30",  // No unit suffix
        NULL
    };

    int32_t retry_after = ik_openai_get_retry_after(headers);
    // parse_duration should return -1 for number without unit
    ck_assert_int_eq(retry_after, -1);
}
END_TEST

/**
 * Test: Header with mixed whitespace (spaces and tabs)
 * Covers line 163-165: while loop with both space and tab conditions
 */
START_TEST(test_retry_after_mixed_whitespace)
{
    const char *headers[] = {
        "x-ratelimit-reset-requests:  \t \t30s",  // Mixed spaces and tabs
        NULL
    };

    int32_t retry_after = ik_openai_get_retry_after(headers);
    ck_assert_int_eq(retry_after, 30);
}
END_TEST

/**
 * Test: Code field is non-string but type field contains content_filter
 * Covers line 78: false branch of yyjson_is_str(code_val) ternary
 * Combined with line 82: is_content_filter(type) when is_content_filter(code) is false
 */
START_TEST(test_handle_error_code_nonstring_type_content_filter)
{
    // code is a number (non-string), type contains content_filter
    const char *json = "{\"error\": {\"message\": \"Filtered\", \"code\": 123, \"type\": \"content_filter\"}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 400, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_CONTENT_FILTER);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *openai_error_coverage_suite(void)
{
    Suite *s = suite_create("OpenAI Error Coverage");

    TCase *tc_handle = tcase_create("Handle Error Coverage");
    tcase_set_timeout(tc_handle, 30);
    tcase_add_unchecked_fixture(tc_handle, setup, teardown);
    tcase_add_test(tc_handle, test_handle_error_missing_code_field);
    tcase_add_test(tc_handle, test_handle_error_missing_type_field);
    tcase_add_test(tc_handle, test_handle_error_missing_code_and_type);
    tcase_add_test(tc_handle, test_handle_error_code_null_ternary);
    tcase_add_test(tc_handle, test_handle_error_content_filter_type_only);
    tcase_add_test(tc_handle, test_handle_error_code_no_match);
    tcase_add_test(tc_handle, test_handle_error_both_null_for_content_filter);
    tcase_add_test(tc_handle, test_handle_error_code_nonstring_type_content_filter);
    suite_add_tcase(s, tc_handle);

    TCase *tc_retry = tcase_create("Retry After Coverage");
    tcase_set_timeout(tc_retry, 30);
    tcase_add_unchecked_fixture(tc_retry, setup, teardown);
    tcase_add_test(tc_retry, test_retry_after_no_whitespace);
    tcase_add_test(tc_retry, test_retry_after_tokens_no_whitespace);
    tcase_add_test(tc_retry, test_retry_after_with_tab);
    tcase_add_test(tc_retry, test_retry_after_tokens_with_tab);
    tcase_add_test(tc_retry, test_retry_after_requests_valid_tokens_invalid);
    tcase_add_test(tc_retry, test_retry_after_requests_invalid_tokens_valid);
    tcase_add_test(tc_retry, test_retry_after_minutes_unit);
    tcase_add_test(tc_retry, test_retry_after_non_matching_header);
    tcase_add_test(tc_retry, test_retry_after_equal_values);
    tcase_add_test(tc_retry, test_retry_after_requests_greater_than_tokens);
    tcase_add_test(tc_retry, test_retry_after_number_no_unit);
    tcase_add_test(tc_retry, test_retry_after_mixed_whitespace);
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
