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
 * Test Suite Configuration
 */

static Suite *error_handling_suite(void)
{
    Suite *s = suite_create("Provider Error Handling");

    TCase *tc_core = tcase_create("Core");
    tcase_set_timeout(tc_core, 30);
    tcase_add_test(tc_core, test_error_is_retryable);
    tcase_add_test(tc_core, test_error_user_message);
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
