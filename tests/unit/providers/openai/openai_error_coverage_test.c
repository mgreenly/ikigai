#include "../../../test_constants.h"
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
START_TEST(test_handle_error_missing_code_field) {
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
START_TEST(test_handle_error_missing_type_field) {
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
START_TEST(test_handle_error_missing_code_and_type) {
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
START_TEST(test_handle_error_code_null_ternary) {
    // This is covered by missing_code_field test, but making explicit
    const char *json = "{\"error\": {\"type\": \"error\"}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 500, json, &category);
    ck_assert(!is_err(&r));
    // When code is NULL, code_val is NULL, so yyjson_is_str returns false
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}
END_TEST

/**
 * Test: Content filter detected via type when code doesn't match
 * Covers line 82: is_content_filter(type) when is_content_filter(code) is false
 */
START_TEST(test_handle_error_content_filter_type_only) {
    // JSON with content_filter in type but not in code
    const char *json =
        "{\"error\": {\"message\": \"Filtered\", \"type\": \"content_filter\", \"code\": \"other_code\"}}";
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
START_TEST(test_handle_error_code_no_match) {
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
START_TEST(test_handle_error_both_null_for_content_filter) {
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
 * Test: Code field is non-string but type field contains content_filter
 * Covers line 78: false branch of yyjson_is_str(code_val) ternary
 * Combined with line 82: is_content_filter(type) when is_content_filter(code) is false
 */
START_TEST(test_handle_error_code_nonstring_type_content_filter) {
    // code is a number (non-string), type contains content_filter
    const char *json = "{\"error\": {\"message\": \"Filtered\", \"code\": 123, \"type\": \"content_filter\"}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 400, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_CONTENT_FILTER);
}
END_TEST

/**
 * Test: Both code and type fields are non-strings
 * Covers line 78-79: both ternary false branches simultaneously
 */
START_TEST(test_handle_error_both_code_and_type_nonstring) {
    // Both code and type are numbers (non-strings)
    const char *json = "{\"error\": {\"message\": \"Error\", \"code\": 123, \"type\": 456}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 500, json, &category);
    ck_assert(!is_err(&r));
    // Should fall back to status-based category
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}
END_TEST

/**
 * Test: Error object with fields in different order to exercise yyjson_obj_get differently
 * The order of fields affects the internal iteration in yyjson_obj_get
 */
START_TEST(test_handle_error_field_order_variation) {
    // Fields in order: type, message, code (different from other tests)
    const char *json = "{\"error\": {\"type\": \"error\", \"message\": \"Test\", \"code\": \"test_code\"}}";
    ik_error_category_t category;

    res_t r = ik_openai_handle_error(test_ctx, 500, json, &category);
    ck_assert(!is_err(&r));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *openai_error_coverage_suite(void)
{
    Suite *s = suite_create("OpenAI Error Coverage");

    TCase *tc_handle = tcase_create("Handle Error Coverage");
    tcase_set_timeout(tc_handle, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_handle, setup, teardown);
    tcase_add_test(tc_handle, test_handle_error_missing_code_field);
    tcase_add_test(tc_handle, test_handle_error_missing_type_field);
    tcase_add_test(tc_handle, test_handle_error_missing_code_and_type);
    tcase_add_test(tc_handle, test_handle_error_code_null_ternary);
    tcase_add_test(tc_handle, test_handle_error_content_filter_type_only);
    tcase_add_test(tc_handle, test_handle_error_code_no_match);
    tcase_add_test(tc_handle, test_handle_error_both_null_for_content_filter);
    tcase_add_test(tc_handle, test_handle_error_code_nonstring_type_content_filter);
    tcase_add_test(tc_handle, test_handle_error_both_code_and_type_nonstring);
    tcase_add_test(tc_handle, test_handle_error_field_order_variation);
    suite_add_tcase(s, tc_handle);

    return s;
}

int main(void)
{
    Suite *s = openai_error_coverage_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/providers/openai/openai_error_coverage_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
