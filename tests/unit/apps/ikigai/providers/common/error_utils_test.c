#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/providers/common/error_utils.h"
#include "tests/helpers/test_utils_helper.h"

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

/**
 * Unit tests for provider error utilities - Category Names and Retryability
 *
 * Tests error categorization and retryability checking.
 */

/**
 * Category Name Tests
 */

/**
 * Retryability Tests
 */

START_TEST(test_retryable_rate_limit) {
    ck_assert(ik_error_is_retryable(IK_ERR_CAT_RATE_LIMIT));
}

END_TEST

START_TEST(test_retryable_server) {
    ck_assert(ik_error_is_retryable(IK_ERR_CAT_SERVER));
}

END_TEST

START_TEST(test_retryable_timeout) {
    ck_assert(ik_error_is_retryable(IK_ERR_CAT_TIMEOUT));
}

END_TEST

START_TEST(test_retryable_network) {
    ck_assert(ik_error_is_retryable(IK_ERR_CAT_NETWORK));
}

END_TEST

START_TEST(test_not_retryable_auth) {
    ck_assert(!ik_error_is_retryable(IK_ERR_CAT_AUTH));
}

END_TEST

START_TEST(test_not_retryable_invalid_arg) {
    ck_assert(!ik_error_is_retryable(IK_ERR_CAT_INVALID_ARG));
}

END_TEST

START_TEST(test_not_retryable_not_found) {
    ck_assert(!ik_error_is_retryable(IK_ERR_CAT_NOT_FOUND));
}

END_TEST

START_TEST(test_not_retryable_content_filter) {
    ck_assert(!ik_error_is_retryable(IK_ERR_CAT_CONTENT_FILTER));
}

END_TEST

START_TEST(test_not_retryable_unknown) {
    ck_assert(!ik_error_is_retryable(IK_ERR_CAT_UNKNOWN));
}

END_TEST

START_TEST(test_not_retryable_invalid_category) {
    /* Test with invalid category value (999) */
    ck_assert(!ik_error_is_retryable((ik_error_category_t)999));
}

END_TEST

/**
 * Test Suite Configuration
 */

static Suite *error_suite(void)
{
    Suite *s = suite_create("Provider Error Utilities");

    /* Retryability tests */
    TCase *tc_retry = tcase_create("Retryability");
    tcase_set_timeout(tc_retry, IK_TEST_TIMEOUT);
    tcase_add_test(tc_retry, test_retryable_rate_limit);
    tcase_add_test(tc_retry, test_retryable_server);
    tcase_add_test(tc_retry, test_retryable_timeout);
    tcase_add_test(tc_retry, test_retryable_network);
    tcase_add_test(tc_retry, test_not_retryable_auth);
    tcase_add_test(tc_retry, test_not_retryable_invalid_arg);
    tcase_add_test(tc_retry, test_not_retryable_not_found);
    tcase_add_test(tc_retry, test_not_retryable_content_filter);
    tcase_add_test(tc_retry, test_not_retryable_unknown);
    tcase_add_test(tc_retry, test_not_retryable_invalid_category);
    suite_add_tcase(s, tc_retry);

    return s;
}

int main(void)
{
    Suite *s = error_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/common/error_utils_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
