#include "../../../test_constants.h"

#include "tools/web_search_google/http_utils.h"

#include <check.h>
#include <string.h>
#include <talloc.h>

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

START_TEST(test_write_callback_success) {
    struct response_buffer buf;
    buf.ctx = test_ctx;
    buf.data = NULL;
    buf.size = 0;

    char data[] = "test data";
    size_t result = write_callback(data, 1, strlen(data), &buf);

    ck_assert_uint_eq(result, strlen(data));
    ck_assert_uint_eq(buf.size, strlen(data));
    ck_assert_str_eq(buf.data, "test data");
}

END_TEST

START_TEST(test_write_callback_multiple_calls) {
    struct response_buffer buf;
    buf.ctx = test_ctx;
    buf.data = NULL;
    buf.size = 0;

    char data1[] = "first ";
    char data2[] = "second";

    size_t result1 = write_callback(data1, 1, strlen(data1), &buf);
    ck_assert_uint_eq(result1, strlen(data1));

    size_t result2 = write_callback(data2, 1, strlen(data2), &buf);
    ck_assert_uint_eq(result2, strlen(data2));

    ck_assert_uint_eq(buf.size, strlen(data1) + strlen(data2));
    ck_assert_str_eq(buf.data, "first second");
}

END_TEST

START_TEST(test_write_callback_size_nmemb) {
    struct response_buffer buf;
    buf.ctx = test_ctx;
    buf.data = NULL;
    buf.size = 0;

    char data[] = "abcd";
    size_t result = write_callback(data, 2, 2, &buf);

    ck_assert_uint_eq(result, 4);
    ck_assert_uint_eq(buf.size, 4);
}

END_TEST

START_TEST(test_url_encode_basic) {
    char *result = url_encode(test_ctx, "hello world");
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "hello%20world");
}

END_TEST

START_TEST(test_url_encode_special_chars) {
    char *result = url_encode(test_ctx, "a+b=c&d");
    ck_assert_ptr_nonnull(result);
    ck_assert(strstr(result, "%") != NULL);
}

END_TEST

START_TEST(test_url_encode_empty) {
    char *result = url_encode(test_ctx, "");
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "");
}

END_TEST

START_TEST(test_url_encode_no_encoding_needed) {
    char *result = url_encode(test_ctx, "simple");
    ck_assert_ptr_nonnull(result);
    ck_assert_str_eq(result, "simple");
}

END_TEST

static Suite *http_utils_suite(void)
{
    Suite *s = suite_create("WebSearchGoogleHttpUtils");

    TCase *tc_write = tcase_create("WriteCallback");
    tcase_set_timeout(tc_write, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_write, setup, teardown);
    tcase_add_test(tc_write, test_write_callback_success);
    tcase_add_test(tc_write, test_write_callback_multiple_calls);
    tcase_add_test(tc_write, test_write_callback_size_nmemb);
    suite_add_tcase(s, tc_write);

    TCase *tc_encode = tcase_create("UrlEncode");
    tcase_set_timeout(tc_encode, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_encode, setup, teardown);
    tcase_add_test(tc_encode, test_url_encode_basic);
    tcase_add_test(tc_encode, test_url_encode_special_chars);
    tcase_add_test(tc_encode, test_url_encode_empty);
    tcase_add_test(tc_encode, test_url_encode_no_encoding_needed);
    suite_add_tcase(s, tc_encode);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = http_utils_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/tools/web_search_google/http_utils_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
