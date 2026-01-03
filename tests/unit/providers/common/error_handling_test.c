/**
 * @file test_error_handling.c
 * @brief Unit tests for provider error handling
 *
 * Tests error category mapping and retryability logic.
 * Simplified tests focusing on core error handling contracts.
 */

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "providers/provider.h"
#include "providers/common/error_utils.h"

/**
 * Error Category Name Tests
 */

START_TEST(test_error_category_names) {
    ck_assert_str_eq(ik_error_category_name(IK_ERR_CAT_AUTH), "authentication");
    ck_assert_str_eq(ik_error_category_name(IK_ERR_CAT_RATE_LIMIT), "rate_limit");
    ck_assert_str_eq(ik_error_category_name(IK_ERR_CAT_SERVER), "server_error");
    ck_assert_str_eq(ik_error_category_name(IK_ERR_CAT_NETWORK), "network_error");
    ck_assert_str_eq(ik_error_category_name(IK_ERR_CAT_INVALID_ARG), "invalid_argument");
}
END_TEST
/**
 * Retryability Tests
 */

START_TEST(test_error_is_retryable) {
    /* Retryable categories */
    ck_assert(ik_error_is_retryable(IK_ERR_CAT_RATE_LIMIT));
    ck_assert(ik_error_is_retryable(IK_ERR_CAT_SERVER));
    ck_assert(ik_error_is_retryable(IK_ERR_CAT_NETWORK));
    ck_assert(ik_error_is_retryable(IK_ERR_CAT_TIMEOUT));

    /* Non-retryable categories */
    ck_assert(!ik_error_is_retryable(IK_ERR_CAT_AUTH));
    ck_assert(!ik_error_is_retryable(IK_ERR_CAT_INVALID_ARG));
    ck_assert(!ik_error_is_retryable(IK_ERR_CAT_NOT_FOUND));
    ck_assert(!ik_error_is_retryable(IK_ERR_CAT_CONTENT_FILTER));
}

END_TEST
/**
 * User Message Tests
 *
 * Verify that user-facing error messages are generated correctly.
 */

START_TEST(test_error_user_message) {
    TALLOC_CTX *ctx = talloc_new(NULL);

    /* Test rate limit message */
    char *msg = ik_error_user_message(ctx, "anthropic", IK_ERR_CAT_RATE_LIMIT, NULL);
    ck_assert_ptr_nonnull(msg);
    ck_assert(strstr(msg, "rate") != NULL || strstr(msg, "limit") != NULL || strstr(msg, "Rate") != NULL);

    /* Test auth message */
    msg = ik_error_user_message(ctx, "openai", IK_ERR_CAT_AUTH, NULL);
    ck_assert_ptr_nonnull(msg);
    ck_assert(strstr(msg, "API") != NULL || strstr(msg, "Authentication") != NULL || strstr(msg, "key") != NULL);

    /* Test server error message */
    msg = ik_error_user_message(ctx, "google", IK_ERR_CAT_SERVER, NULL);
    ck_assert_ptr_nonnull(msg);
    ck_assert(strstr(msg, "server") != NULL || strstr(msg, "error") != NULL);

    talloc_free(ctx);
}

END_TEST
/**
 * Retry Delay Calculation Tests
 *
 * Verify exponential backoff with jitter for async retry via event loop.
 */

START_TEST(test_retry_delay_calculation) {
    /* Provider-suggested delay takes precedence */
    int64_t delay = ik_error_calc_retry_delay_ms(1, 5000);
    ck_assert_int_eq(delay, 5000);

    /* Exponential backoff with jitter when no suggestion */
    /* Attempt 1: 1000ms + jitter (0-1000ms) = 1000-2000ms */
    delay = ik_error_calc_retry_delay_ms(1, -1);
    ck_assert(delay >= 1000);
    ck_assert(delay <= 2000);

    /* Attempt 2: 2000ms + jitter (0-1000ms) = 2000-3000ms */
    delay = ik_error_calc_retry_delay_ms(2, -1);
    ck_assert(delay >= 2000);
    ck_assert(delay <= 3000);

    /* Attempt 3: 4000ms + jitter (0-1000ms) = 4000-5000ms */
    delay = ik_error_calc_retry_delay_ms(3, -1);
    ck_assert(delay >= 4000);
    ck_assert(delay <= 5000);
}

END_TEST

/**
 * Test Suite Configuration
 */

static Suite *error_handling_suite(void)
{
    Suite *s = suite_create("Provider Error Handling");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);
    tcase_add_test(tc_core, test_error_category_names);
    tcase_add_test(tc_core, test_error_is_retryable);
    tcase_add_test(tc_core, test_error_user_message);
    tcase_add_test(tc_core, test_retry_delay_calculation);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    Suite *s = error_handling_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
