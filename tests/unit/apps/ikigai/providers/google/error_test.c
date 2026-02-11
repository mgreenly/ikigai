#include "tests/test_constants.h"
/**
 * @file error_test.c
 * @brief Unit tests for Google error handling
 */

#include <check.h>
#include <talloc.h>
#include "apps/ikigai/providers/google/error.h"
#include "apps/ikigai/providers/provider.h"
#include "shared/wrapper.h"

static TALLOC_CTX *test_ctx;

// Mocking flags
static bool mock_yyjson_doc_get_root_null = false;
static bool mock_yyjson_get_str_null = false;

// Mock yyjson_doc_get_root_ to return NULL
yyjson_val *yyjson_doc_get_root_(yyjson_doc *doc)
{
    if (mock_yyjson_doc_get_root_null) {
        return NULL;
    }
    return yyjson_doc_get_root(doc);
}

// Mock yyjson_get_str_ to return NULL
const char *yyjson_get_str_(yyjson_val *val)
{
    if (mock_yyjson_get_str_null) {
        return NULL;
    }
    return yyjson_get_str(val);
}

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    mock_yyjson_doc_get_root_null = false;
    mock_yyjson_get_str_null = false;
}

static void teardown(void)
{
    talloc_free(test_ctx);
    mock_yyjson_doc_get_root_null = false;
    mock_yyjson_get_str_null = false;
}

/* ================================================================
 * Error Handling Tests
 * ================================================================ */

START_TEST(test_handle_error_403_auth) {
    const char *body = "{\"error\":{\"code\":403,\"message\":\"API key invalid\",\"status\":\"PERMISSION_DENIED\"}}";
    ik_error_category_t category;

    res_t result = ik_google_handle_error(test_ctx, 403, body, &category);
    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_AUTH);
}
END_TEST

START_TEST(test_handle_error_429_rate_limit) {
    const char *body =
        "{\"error\":{\"code\":429,\"message\":\"Rate limit exceeded\",\"status\":\"RESOURCE_EXHAUSTED\"}}";
    ik_error_category_t category;

    res_t result = ik_google_handle_error(test_ctx, 429, body, &category);
    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_RATE_LIMIT);
}

END_TEST

START_TEST(test_handle_error_504_timeout) {
    const char *body = "{\"error\":{\"code\":504,\"message\":\"Gateway timeout\",\"status\":\"DEADLINE_EXCEEDED\"}}";
    ik_error_category_t category;

    res_t result = ik_google_handle_error(test_ctx, 504, body, &category);
    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_TIMEOUT);
}

END_TEST

START_TEST(test_handle_error_400_invalid_arg) {
    const char *body = "{\"error\":{\"code\":400,\"message\":\"Invalid argument\",\"status\":\"INVALID_ARGUMENT\"}}";
    ik_error_category_t category;

    res_t result = ik_google_handle_error(test_ctx, 400, body, &category);
    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_INVALID_ARG);
}

END_TEST

START_TEST(test_handle_error_404_not_found) {
    const char *body = "{\"error\":{\"code\":404,\"message\":\"Model not found\",\"status\":\"NOT_FOUND\"}}";
    ik_error_category_t category;

    res_t result = ik_google_handle_error(test_ctx, 404, body, &category);
    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_NOT_FOUND);
}

END_TEST

START_TEST(test_handle_error_500_server) {
    const char *body = "{\"error\":{\"code\":500,\"message\":\"Internal error\",\"status\":\"INTERNAL\"}}";
    ik_error_category_t category;

    res_t result = ik_google_handle_error(test_ctx, 500, body, &category);
    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}

END_TEST

START_TEST(test_handle_error_503_server) {
    const char *body = "{\"error\":{\"code\":503,\"message\":\"Service unavailable\",\"status\":\"UNAVAILABLE\"}}";
    ik_error_category_t category;

    res_t result = ik_google_handle_error(test_ctx, 503, body, &category);
    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}

END_TEST

START_TEST(test_handle_error_invalid_json) {
    const char *body = "not valid json";
    ik_error_category_t category;

    res_t result = ik_google_handle_error(test_ctx, 500, body, &category);
    ck_assert(is_err(&result));
}

END_TEST

START_TEST(test_handle_error_unknown_status) {
    const char *body = "{\"error\":{\"code\":418,\"message\":\"I'm a teapot\",\"status\":\"UNKNOWN\"}}";
    ik_error_category_t category;

    res_t result = ik_google_handle_error(test_ctx, 418, body, &category);
    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_UNKNOWN);
}

END_TEST

START_TEST(test_handle_error_null_root) {
    // Mock yyjson_doc_get_root_ to return NULL
    const char *body = "{\"error\":{\"code\":500}}";
    ik_error_category_t category;

    mock_yyjson_doc_get_root_null = true;
    res_t result = ik_google_handle_error(test_ctx, 500, body, &category);
    ck_assert(is_err(&result));
}

END_TEST

START_TEST(test_handle_error_with_error_fields) {
    // Test case where error object has status and message fields
    const char *body = "{\"error\":{\"status\":\"PERMISSION_DENIED\",\"message\":\"API key invalid\"}}";
    ik_error_category_t category;

    res_t result = ik_google_handle_error(test_ctx, 403, body, &category);
    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_AUTH);
}

END_TEST

START_TEST(test_handle_error_no_error_object) {
    // Test case with no error object in JSON (valid JSON but not a proper error response)
    const char *body = "{\"someOtherField\":\"value\"}";
    ik_error_category_t category;

    res_t result = ik_google_handle_error(test_ctx, 500, body, &category);
    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_error_suite(void)
{
    Suite *s = suite_create("Google Error Handling");

    TCase *tc_error = tcase_create("Error Handling");
    tcase_set_timeout(tc_error, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_error, setup, teardown);
    tcase_add_test(tc_error, test_handle_error_403_auth);
    tcase_add_test(tc_error, test_handle_error_429_rate_limit);
    tcase_add_test(tc_error, test_handle_error_504_timeout);
    tcase_add_test(tc_error, test_handle_error_400_invalid_arg);
    tcase_add_test(tc_error, test_handle_error_404_not_found);
    tcase_add_test(tc_error, test_handle_error_500_server);
    tcase_add_test(tc_error, test_handle_error_503_server);
    tcase_add_test(tc_error, test_handle_error_invalid_json);
    tcase_add_test(tc_error, test_handle_error_unknown_status);
    tcase_add_test(tc_error, test_handle_error_null_root);
    tcase_add_test(tc_error, test_handle_error_with_error_fields);
    tcase_add_test(tc_error, test_handle_error_no_error_object);
    suite_add_tcase(s, tc_error);

    return s;
}

int main(void)
{
    Suite *s = google_error_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/google/error_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
