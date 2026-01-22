#include "../../../test_constants.h"
/**
 * @file google_create_curl_multi_fail_test.c
 * @brief Unit test for Google provider creation with curl_multi_init failure
 */

#include <check.h>
#include <curl/curl.h>
#include <talloc.h>
#include "providers/google/google.h"
#include "wrapper.h"

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

// Mock curl_multi_init_ to return NULL
CURLM *curl_multi_init_(void)
{
    return NULL;
}

START_TEST(test_create_fails_when_curl_multi_init_fails) {
    ik_provider_t *provider = NULL;
    res_t result = ik_google_create(test_ctx, "test-api-key", &provider);

    ck_assert(is_err(&result));
    ck_assert_ptr_null(provider);
}

END_TEST

static Suite *google_create_curl_multi_fail_suite(void)
{
    Suite *s = suite_create("Google Create Curl Multi Fail");

    TCase *tc_create = tcase_create("Create With Curl Multi Init Failure");
    tcase_set_timeout(tc_create, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_create, setup, teardown);
    tcase_add_test(tc_create, test_create_fails_when_curl_multi_init_fails);
    suite_add_tcase(s, tc_create);

    return s;
}

int main(void)
{
    Suite *s = google_create_curl_multi_fail_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/providers/google/google_create_curl_multi_fail_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
