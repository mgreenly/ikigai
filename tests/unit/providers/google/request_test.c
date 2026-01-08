/**
 * @file request_test.c
 * @brief Unit tests for Google URL and header building
 */

// Disable cast-qual for test literals
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/google/request.h"
#include "providers/provider.h"
#include "providers/request.h"
#include "error.h"

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
 * URL Building Tests
 * ================================================================ */

START_TEST(test_build_url_non_streaming) {
    char *url = NULL;
    res_t r = ik_google_build_url(test_ctx, "https://api.test.com",
                                  "gemini-2.0-flash", "test-key", false, &url);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(url);
    ck_assert_str_eq(url, "https://api.test.com/models/gemini-2.0-flash:generateContent?key=test-key");
}
END_TEST

START_TEST(test_build_url_streaming) {
    char *url = NULL;
    res_t r = ik_google_build_url(test_ctx, "https://api.test.com",
                                  "gemini-2.0-flash", "test-key", true, &url);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(url);
    ck_assert_str_eq(url, "https://api.test.com/models/gemini-2.0-flash:streamGenerateContent?key=test-key&alt=sse");
}

END_TEST
/* ================================================================
 * Header Building Tests
 * ================================================================ */

START_TEST(test_build_headers_non_streaming) {
    char **headers = NULL;
    res_t r = ik_google_build_headers(test_ctx, false, &headers);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(headers);
    ck_assert_str_eq(headers[0], "Content-Type: application/json");
    ck_assert_ptr_null(headers[1]);
}

END_TEST

START_TEST(test_build_headers_streaming) {
    char **headers = NULL;
    res_t r = ik_google_build_headers(test_ctx, true, &headers);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(headers);
    ck_assert_str_eq(headers[0], "Content-Type: application/json");
    ck_assert_str_eq(headers[1], "Accept: text/event-stream");
    ck_assert_ptr_null(headers[2]);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *request_suite(void)
{
    Suite *s = suite_create("Google Request");

    TCase *tc_url = tcase_create("URL Building");
    tcase_set_timeout(tc_url, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_url, setup, teardown);
    tcase_add_test(tc_url, test_build_url_non_streaming);
    tcase_add_test(tc_url, test_build_url_streaming);
    suite_add_tcase(s, tc_url);

    TCase *tc_headers = tcase_create("Header Building");
    tcase_set_timeout(tc_headers, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_headers, setup, teardown);
    tcase_add_test(tc_headers, test_build_headers_non_streaming);
    tcase_add_test(tc_headers, test_build_headers_streaming);
    suite_add_tcase(s, tc_headers);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = request_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}

#pragma GCC diagnostic pop
