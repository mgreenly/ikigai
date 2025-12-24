/**
 * @file error_test.c
 * @brief Unit tests for Google error handling
 */

#include <check.h>
#include <talloc.h>
#include "providers/google/error.h"
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

START_TEST(test_handle_error_403_auth)
{
    const char *body = "{\"error\":{\"code\":403,\"message\":\"API key invalid\",\"status\":\"PERMISSION_DENIED\"}}";
    ik_error_category_t category;

    res_t result = ik_google_handle_error(test_ctx, 403, body, &category);
    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_AUTH);
}
END_TEST

START_TEST(test_handle_error_429_rate_limit)
{
    const char *body = "{\"error\":{\"code\":429,\"message\":\"Rate limit exceeded\",\"status\":\"RESOURCE_EXHAUSTED\"}}";
    ik_error_category_t category;

    res_t result = ik_google_handle_error(test_ctx, 429, body, &category);
    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_RATE_LIMIT);
}
END_TEST

START_TEST(test_handle_error_504_timeout)
{
    const char *body = "{\"error\":{\"code\":504,\"message\":\"Gateway timeout\",\"status\":\"DEADLINE_EXCEEDED\"}}";
    ik_error_category_t category;

    res_t result = ik_google_handle_error(test_ctx, 504, body, &category);
    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_TIMEOUT);
}
END_TEST

START_TEST(test_handle_error_400_invalid_arg)
{
    const char *body = "{\"error\":{\"code\":400,\"message\":\"Invalid argument\",\"status\":\"INVALID_ARGUMENT\"}}";
    ik_error_category_t category;

    res_t result = ik_google_handle_error(test_ctx, 400, body, &category);
    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_INVALID_ARG);
}
END_TEST

START_TEST(test_handle_error_404_not_found)
{
    const char *body = "{\"error\":{\"code\":404,\"message\":\"Model not found\",\"status\":\"NOT_FOUND\"}}";
    ik_error_category_t category;

    res_t result = ik_google_handle_error(test_ctx, 404, body, &category);
    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_NOT_FOUND);
}
END_TEST

START_TEST(test_handle_error_500_server)
{
    const char *body = "{\"error\":{\"code\":500,\"message\":\"Internal error\",\"status\":\"INTERNAL\"}}";
    ik_error_category_t category;

    res_t result = ik_google_handle_error(test_ctx, 500, body, &category);
    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}
END_TEST

START_TEST(test_handle_error_503_server)
{
    const char *body = "{\"error\":{\"code\":503,\"message\":\"Service unavailable\",\"status\":\"UNAVAILABLE\"}}";
    ik_error_category_t category;

    res_t result = ik_google_handle_error(test_ctx, 503, body, &category);
    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
}
END_TEST

START_TEST(test_handle_error_invalid_json)
{
    const char *body = "not valid json";
    ik_error_category_t category;

    res_t result = ik_google_handle_error(test_ctx, 500, body, &category);
    ck_assert(is_err(&result));
}
END_TEST

/* ================================================================
 * Retry-After Tests
 * ================================================================ */

START_TEST(test_get_retry_after_60s)
{
    const char *body = "{\"error\":{\"code\":429,\"status\":\"RESOURCE_EXHAUSTED\"},\"retryDelay\":\"60s\"}";

    int32_t retry = ik_google_get_retry_after(body);
    ck_assert_int_eq(retry, 60);
}
END_TEST

START_TEST(test_get_retry_after_30s)
{
    const char *body = "{\"error\":{\"code\":429,\"status\":\"RESOURCE_EXHAUSTED\"},\"retryDelay\":\"30s\"}";

    int32_t retry = ik_google_get_retry_after(body);
    ck_assert_int_eq(retry, 30);
}
END_TEST

START_TEST(test_get_retry_after_not_present)
{
    const char *body = "{\"error\":{\"code\":429,\"status\":\"RESOURCE_EXHAUSTED\"}}";

    int32_t retry = ik_google_get_retry_after(body);
    ck_assert_int_eq(retry, -1);
}
END_TEST

START_TEST(test_get_retry_after_invalid_json)
{
    const char *body = "not valid json";

    int32_t retry = ik_google_get_retry_after(body);
    ck_assert_int_eq(retry, -1);
}
END_TEST

START_TEST(test_get_retry_after_null_body)
{
    int32_t retry = ik_google_get_retry_after(NULL);
    ck_assert_int_eq(retry, -1);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_error_suite(void)
{
    Suite *s = suite_create("Google Error Handling");

    TCase *tc_error = tcase_create("Error Handling");
    tcase_add_unchecked_fixture(tc_error, setup, teardown);
    tcase_add_test(tc_error, test_handle_error_403_auth);
    tcase_add_test(tc_error, test_handle_error_429_rate_limit);
    tcase_add_test(tc_error, test_handle_error_504_timeout);
    tcase_add_test(tc_error, test_handle_error_400_invalid_arg);
    tcase_add_test(tc_error, test_handle_error_404_not_found);
    tcase_add_test(tc_error, test_handle_error_500_server);
    tcase_add_test(tc_error, test_handle_error_503_server);
    tcase_add_test(tc_error, test_handle_error_invalid_json);
    suite_add_tcase(s, tc_error);

    TCase *tc_retry = tcase_create("Retry After");
    tcase_add_unchecked_fixture(tc_retry, setup, teardown);
    tcase_add_test(tc_retry, test_get_retry_after_60s);
    tcase_add_test(tc_retry, test_get_retry_after_30s);
    tcase_add_test(tc_retry, test_get_retry_after_not_present);
    tcase_add_test(tc_retry, test_get_retry_after_invalid_json);
    tcase_add_test(tc_retry, test_get_retry_after_null_body);
    suite_add_tcase(s, tc_retry);

    return s;
}

int main(void)
{
    Suite *s = google_error_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
