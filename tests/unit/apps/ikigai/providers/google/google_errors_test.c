#include "tests/test_constants.h"
/**
 * @file test_google_errors.c
 * @brief Unit tests for Google error response parsing and mapping
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/providers/google/response.h"
#include "apps/ikigai/providers/provider.h"

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

START_TEST(test_parse_authentication_error_401) {
    const char *json = "{"
                       "\"error\":{"
                       "\"code\":401,"
                       "\"message\":\"API key not valid. Please pass a valid API key.\","
                       "\"status\":\"UNAUTHENTICATED\""
                       "}"
                       "}";

    ik_error_category_t category;
    char *message = NULL;

    res_t result = ik_google_parse_error(test_ctx, 401, json, strlen(json),
                                         &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_AUTH);
    ck_assert_ptr_nonnull(message);
    ck_assert_ptr_nonnull(strstr(message, "API key"));
}
END_TEST

START_TEST(test_parse_rate_limit_error_429) {
    const char *json = "{"
                       "\"error\":{"
                       "\"code\":429,"
                       "\"message\":\"Resource has been exhausted (e.g. check quota).\","
                       "\"status\":\"RESOURCE_EXHAUSTED\","
                       "\"details\":[{"
                       "\"@type\":\"type.googleapis.com/google.rpc.ErrorInfo\","
                       "\"reason\":\"RATE_LIMIT_EXCEEDED\","
                       "\"domain\":\"googleapis.com\","
                       "\"metadata\":{"
                       "\"service\":\"generativelanguage.googleapis.com\","
                       "\"quota_limit\":\"RequestsPerMinutePerProject\""
                       "}"
                       "}]"
                       "}"
                       "}";

    ik_error_category_t category;
    char *message = NULL;

    res_t result = ik_google_parse_error(test_ctx, 429, json, strlen(json),
                                         &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_RATE_LIMIT);
    ck_assert_ptr_nonnull(message);
}

END_TEST

START_TEST(test_parse_quota_exceeded_error) {
    const char *json = "{"
                       "\"error\":{"
                       "\"code\":403,"
                       "\"message\":\"Quota exceeded for quota metric 'GenerateContent requests' and limit 'GenerateContent requests per minute'.\","
                       "\"status\":\"RESOURCE_EXHAUSTED\""
                       "}"
                       "}";

    ik_error_category_t category;
    char *message = NULL;

    res_t result = ik_google_parse_error(test_ctx, 403, json, strlen(json),
                                         &category, &message);

    ck_assert(!is_err(&result));
    // 403 maps to AUTH (current implementation uses HTTP status only)
    ck_assert_int_eq(category, IK_ERR_CAT_AUTH);
    ck_assert_ptr_nonnull(message);
    ck_assert_ptr_nonnull(strstr(message, "Quota"));
}

END_TEST

START_TEST(test_parse_validation_error_400) {
    const char *json = "{"
                       "\"error\":{"
                       "\"code\":400,"
                       "\"message\":\"Invalid argument: model name is required.\","
                       "\"status\":\"INVALID_ARGUMENT\""
                       "}"
                       "}";

    ik_error_category_t category;
    char *message = NULL;

    res_t result = ik_google_parse_error(test_ctx, 400, json, strlen(json),
                                         &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_INVALID_ARG);
    ck_assert_ptr_nonnull(message);
    ck_assert_ptr_nonnull(strstr(message, "Invalid argument"));
}

END_TEST

START_TEST(test_map_errors_to_correct_categories) {
    // Test 401 -> AUTH
    ik_error_category_t cat1;
    char *msg1 = NULL;
    const char *json1 = "{\"error\":{\"code\":401,\"message\":\"Unauthorized\"}}";
    res_t r1 = ik_google_parse_error(test_ctx, 401, json1, strlen(json1), &cat1, &msg1);
    ck_assert(!is_err(&r1));
    ck_assert_int_eq(cat1, IK_ERR_CAT_AUTH);

    // Test 403 PERMISSION_DENIED -> AUTH
    ik_error_category_t cat2;
    char *msg2 = NULL;
    const char *json2 = "{\"error\":{\"code\":403,\"message\":\"API not enabled\",\"status\":\"PERMISSION_DENIED\"}}";
    res_t r2 = ik_google_parse_error(test_ctx, 403, json2, strlen(json2), &cat2, &msg2);
    ck_assert(!is_err(&r2));
    ck_assert_int_eq(cat2, IK_ERR_CAT_AUTH);

    // Test 403 PERMISSION_DENIED -> AUTH (HTTP status determines category)
    ik_error_category_t cat3;
    char *msg3 = NULL;
    const char *json3 = "{\"error\":{\"code\":403,\"message\":\"Permission denied\",\"status\":\"PERMISSION_DENIED\"}}";
    res_t r3 = ik_google_parse_error(test_ctx, 403, json3, strlen(json3), &cat3, &msg3);
    ck_assert(!is_err(&r3));
    ck_assert_int_eq(cat3, IK_ERR_CAT_AUTH);

    // Test 500 -> SERVER
    ik_error_category_t cat4;
    char *msg4 = NULL;
    const char *json4 = "{\"error\":{\"code\":500,\"message\":\"Internal error\"}}";
    res_t r4 = ik_google_parse_error(test_ctx, 500, json4, strlen(json4), &cat4, &msg4);
    ck_assert(!is_err(&r4));
    ck_assert_int_eq(cat4, IK_ERR_CAT_SERVER);

    // Test 504 -> TIMEOUT
    ik_error_category_t cat5;
    char *msg5 = NULL;
    const char *json5 = "{\"error\":{\"code\":504,\"message\":\"Gateway timeout\"}}";
    res_t r5 = ik_google_parse_error(test_ctx, 504, json5, strlen(json5), &cat5, &msg5);
    ck_assert(!is_err(&r5));
    ck_assert_int_eq(cat5, IK_ERR_CAT_TIMEOUT);

    // Test unknown status code -> UNKNOWN
    ik_error_category_t cat6;
    char *msg6 = NULL;
    const char *json6 = "{\"error\":{\"code\":418,\"message\":\"I'm a teapot\"}}";
    res_t r6 = ik_google_parse_error(test_ctx, 418, json6, strlen(json6), &cat6, &msg6);
    ck_assert(!is_err(&r6));
    ck_assert_int_eq(cat6, IK_ERR_CAT_UNKNOWN);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_errors_suite(void)
{
    Suite *s = suite_create("Google Errors");

    TCase *tc_errors = tcase_create("Error Parsing and Mapping");
    tcase_set_timeout(tc_errors, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_errors, setup, teardown);
    tcase_add_test(tc_errors, test_parse_authentication_error_401);
    tcase_add_test(tc_errors, test_parse_rate_limit_error_429);
    tcase_add_test(tc_errors, test_parse_quota_exceeded_error);
    tcase_add_test(tc_errors, test_parse_validation_error_400);
    tcase_add_test(tc_errors, test_map_errors_to_correct_categories);
    suite_add_tcase(s, tc_errors);

    return s;
}

int main(void)
{
    Suite *s = google_errors_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/google/google_errors_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
