#include "../../../../src/providers/provider.h"
#include "../../../../src/providers/common/error_utils.h"
#include "../../../test_utils.h"

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

/**
 * Unit tests for provider error utilities
 *
 * Tests error categorization, retryability checking, user message generation,
 * and retry delay calculation for async event loop integration.
 */

/* Test context */
static TALLOC_CTX *test_ctx = NULL;

/* Setup/teardown */
static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
    test_ctx = NULL;
}

/**
 * Category Name Tests
 */

START_TEST(test_category_name_auth) {
    const char *name = ik_error_category_name(IK_ERR_CAT_AUTH);
    ck_assert_str_eq(name, "authentication");
}
END_TEST START_TEST(test_category_name_rate_limit)
{
    const char *name = ik_error_category_name(IK_ERR_CAT_RATE_LIMIT);
    ck_assert_str_eq(name, "rate_limit");
}

END_TEST START_TEST(test_category_name_invalid_arg)
{
    const char *name = ik_error_category_name(IK_ERR_CAT_INVALID_ARG);
    ck_assert_str_eq(name, "invalid_argument");
}

END_TEST START_TEST(test_category_name_not_found)
{
    const char *name = ik_error_category_name(IK_ERR_CAT_NOT_FOUND);
    ck_assert_str_eq(name, "not_found");
}

END_TEST START_TEST(test_category_name_server)
{
    const char *name = ik_error_category_name(IK_ERR_CAT_SERVER);
    ck_assert_str_eq(name, "server_error");
}

END_TEST START_TEST(test_category_name_timeout)
{
    const char *name = ik_error_category_name(IK_ERR_CAT_TIMEOUT);
    ck_assert_str_eq(name, "timeout");
}

END_TEST START_TEST(test_category_name_content_filter)
{
    const char *name = ik_error_category_name(IK_ERR_CAT_CONTENT_FILTER);
    ck_assert_str_eq(name, "content_filter");
}

END_TEST START_TEST(test_category_name_network)
{
    const char *name = ik_error_category_name(IK_ERR_CAT_NETWORK);
    ck_assert_str_eq(name, "network_error");
}

END_TEST START_TEST(test_category_name_unknown)
{
    const char *name = ik_error_category_name(IK_ERR_CAT_UNKNOWN);
    ck_assert_str_eq(name, "unknown");
}

END_TEST START_TEST(test_category_name_invalid)
{
    /* Test with invalid category value (999) */
    const char *name = ik_error_category_name((ik_error_category_t)999);
    ck_assert_str_eq(name, "unknown");
}

END_TEST
/**
 * Retryability Tests
 */

START_TEST(test_retryable_rate_limit)
{
    ck_assert(ik_error_is_retryable(IK_ERR_CAT_RATE_LIMIT));
}

END_TEST START_TEST(test_retryable_server)
{
    ck_assert(ik_error_is_retryable(IK_ERR_CAT_SERVER));
}

END_TEST START_TEST(test_retryable_timeout)
{
    ck_assert(ik_error_is_retryable(IK_ERR_CAT_TIMEOUT));
}

END_TEST START_TEST(test_retryable_network)
{
    ck_assert(ik_error_is_retryable(IK_ERR_CAT_NETWORK));
}

END_TEST START_TEST(test_not_retryable_auth)
{
    ck_assert(!ik_error_is_retryable(IK_ERR_CAT_AUTH));
}

END_TEST START_TEST(test_not_retryable_invalid_arg)
{
    ck_assert(!ik_error_is_retryable(IK_ERR_CAT_INVALID_ARG));
}

END_TEST START_TEST(test_not_retryable_not_found)
{
    ck_assert(!ik_error_is_retryable(IK_ERR_CAT_NOT_FOUND));
}

END_TEST START_TEST(test_not_retryable_content_filter)
{
    ck_assert(!ik_error_is_retryable(IK_ERR_CAT_CONTENT_FILTER));
}

END_TEST START_TEST(test_not_retryable_unknown)
{
    ck_assert(!ik_error_is_retryable(IK_ERR_CAT_UNKNOWN));
}

END_TEST START_TEST(test_not_retryable_invalid_category)
{
    /* Test with invalid category value (999) */
    ck_assert(!ik_error_is_retryable((ik_error_category_t)999));
}

END_TEST
/**
 * User Message Tests
 */

START_TEST(test_user_message_auth_anthropic)
{
    char *msg = ik_error_user_message(test_ctx, "anthropic", IK_ERR_CAT_AUTH, NULL);
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg,
                     "Authentication failed for anthropic. Check your API key in ANTHROPIC_API_KEY or ~/.config/ikigai/credentials.json");
    ck_assert(talloc_get_size(msg) > 0); /* Verify allocated on context */
}

END_TEST START_TEST(test_user_message_auth_openai)
{
    char *msg = ik_error_user_message(test_ctx, "openai", IK_ERR_CAT_AUTH, NULL);
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg,
                     "Authentication failed for openai. Check your API key in OPENAI_API_KEY or ~/.config/ikigai/credentials.json");
}

END_TEST START_TEST(test_user_message_auth_google)
{
    char *msg = ik_error_user_message(test_ctx, "google", IK_ERR_CAT_AUTH, NULL);
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg,
                     "Authentication failed for google. Check your API key in GOOGLE_API_KEY or ~/.config/ikigai/credentials.json");
}

END_TEST START_TEST(test_user_message_rate_limit_with_detail)
{
    char *msg = ik_error_user_message(test_ctx, "anthropic", IK_ERR_CAT_RATE_LIMIT, "Try again in 60 seconds");
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "Rate limit exceeded for anthropic. Try again in 60 seconds");
}

END_TEST START_TEST(test_user_message_rate_limit_no_detail)
{
    char *msg = ik_error_user_message(test_ctx, "anthropic", IK_ERR_CAT_RATE_LIMIT, NULL);
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "Rate limit exceeded for anthropic.");
}

END_TEST START_TEST(test_user_message_invalid_arg_with_detail)
{
    char *msg = ik_error_user_message(test_ctx, "openai", IK_ERR_CAT_INVALID_ARG, "max_tokens must be positive");
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "Invalid request to openai: max_tokens must be positive");
}

END_TEST START_TEST(test_user_message_invalid_arg_no_detail)
{
    char *msg = ik_error_user_message(test_ctx, "openai", IK_ERR_CAT_INVALID_ARG, NULL);
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "Invalid request to openai");
}

END_TEST START_TEST(test_user_message_not_found_with_detail)
{
    char *msg = ik_error_user_message(test_ctx, "google", IK_ERR_CAT_NOT_FOUND, "gemini-99 does not exist");
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "Model not found on google: gemini-99 does not exist");
}

END_TEST START_TEST(test_user_message_not_found_no_detail)
{
    char *msg = ik_error_user_message(test_ctx, "google", IK_ERR_CAT_NOT_FOUND, NULL);
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "Model not found on google");
}

END_TEST START_TEST(test_user_message_server_with_detail)
{
    char *msg = ik_error_user_message(test_ctx, "anthropic", IK_ERR_CAT_SERVER, "Overloaded");
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "anthropic server error. This is temporary, retrying may succeed. Overloaded");
}

END_TEST START_TEST(test_user_message_server_no_detail)
{
    char *msg = ik_error_user_message(test_ctx, "anthropic", IK_ERR_CAT_SERVER, NULL);
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "anthropic server error. This is temporary, retrying may succeed.");
}

END_TEST START_TEST(test_user_message_timeout)
{
    char *msg = ik_error_user_message(test_ctx, "openai", IK_ERR_CAT_TIMEOUT, NULL);
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "Request to openai timed out. Check network connection.");
}

END_TEST START_TEST(test_user_message_content_filter_with_detail)
{
    char *msg = ik_error_user_message(test_ctx, "google", IK_ERR_CAT_CONTENT_FILTER, "Harmful content detected");
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "Content blocked by google safety filters: Harmful content detected");
}

END_TEST START_TEST(test_user_message_content_filter_no_detail)
{
    char *msg = ik_error_user_message(test_ctx, "google", IK_ERR_CAT_CONTENT_FILTER, NULL);
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "Content blocked by google safety filters");
}

END_TEST START_TEST(test_user_message_network_with_detail)
{
    char *msg = ik_error_user_message(test_ctx, "anthropic", IK_ERR_CAT_NETWORK, "Connection refused");
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "Network error connecting to anthropic: Connection refused");
}

END_TEST START_TEST(test_user_message_network_no_detail)
{
    char *msg = ik_error_user_message(test_ctx, "anthropic", IK_ERR_CAT_NETWORK, NULL);
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "Network error connecting to anthropic");
}

END_TEST START_TEST(test_user_message_unknown_with_detail)
{
    char *msg = ik_error_user_message(test_ctx, "openai", IK_ERR_CAT_UNKNOWN, "Something went wrong");
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "openai error: Something went wrong");
}

END_TEST START_TEST(test_user_message_unknown_no_detail)
{
    char *msg = ik_error_user_message(test_ctx, "openai", IK_ERR_CAT_UNKNOWN, NULL);
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "openai error");
}

END_TEST START_TEST(test_user_message_empty_detail_treated_as_null)
{
    char *msg = ik_error_user_message(test_ctx, "anthropic", IK_ERR_CAT_RATE_LIMIT, "");
    ck_assert_ptr_nonnull(msg);
    /* Empty string should be treated same as NULL - no trailing detail */
    ck_assert_str_eq(msg, "Rate limit exceeded for anthropic.");
}

END_TEST START_TEST(test_user_message_allocated_on_context)
{
    char *msg = ik_error_user_message(test_ctx, "openai", IK_ERR_CAT_SERVER, "Test");
    ck_assert_ptr_nonnull(msg);
    /* Verify message is allocated (has non-zero size) */
    size_t size = talloc_get_size(msg);
    ck_assert(size > 0);
}

END_TEST
/**
 * Retry Delay Calculation Tests
 */

START_TEST(test_retry_delay_provider_suggested)
{
    /* When provider suggests a delay, use it exactly */
    int64_t delay = ik_error_calc_retry_delay_ms(1, 5000);
    ck_assert_int_eq(delay, 5000);

    delay = ik_error_calc_retry_delay_ms(2, 3000);
    ck_assert_int_eq(delay, 3000);

    delay = ik_error_calc_retry_delay_ms(3, 10000);
    ck_assert_int_eq(delay, 10000);
}

END_TEST START_TEST(test_retry_delay_exponential_backoff_attempt_1)
{
    /* Attempt 1: base 1000ms + jitter 0-1000ms = 1000-2000ms */
    int64_t delay = ik_error_calc_retry_delay_ms(1, -1);
    ck_assert(delay >= 1000);
    ck_assert(delay <= 2000);
}

END_TEST START_TEST(test_retry_delay_exponential_backoff_attempt_2)
{
    /* Attempt 2: base 2000ms + jitter 0-1000ms = 2000-3000ms */
    int64_t delay = ik_error_calc_retry_delay_ms(2, -1);
    ck_assert(delay >= 2000);
    ck_assert(delay <= 3000);
}

END_TEST START_TEST(test_retry_delay_exponential_backoff_attempt_3)
{
    /* Attempt 3: base 4000ms + jitter 0-1000ms = 4000-5000ms */
    int64_t delay = ik_error_calc_retry_delay_ms(3, -1);
    ck_assert(delay >= 4000);
    ck_assert(delay <= 5000);
}

END_TEST START_TEST(test_retry_delay_zero_triggers_backoff)
{
    /* Provider suggested delay of 0 triggers exponential backoff */
    int64_t delay = ik_error_calc_retry_delay_ms(1, 0);
    ck_assert(delay >= 1000);
    ck_assert(delay <= 2000);
}

END_TEST START_TEST(test_retry_delay_negative_triggers_backoff)
{
    /* Provider suggested delay of -1 triggers exponential backoff */
    int64_t delay = ik_error_calc_retry_delay_ms(2, -1);
    ck_assert(delay >= 2000);
    ck_assert(delay <= 3000);
}

END_TEST START_TEST(test_retry_delay_jitter_randomness)
{
    /* Multiple calls should produce different results due to jitter */
    int64_t delays[5];
    bool all_different = false;

    for (int i = 0; i < 5; i++) {
        delays[i] = ik_error_calc_retry_delay_ms(1, -1);
    }

    /* Check if at least some values differ (jitter at work) */
    for (int i = 0; i < 4; i++) {
        for (int j = i + 1; j < 5; j++) {
            if (delays[i] != delays[j]) {
                all_different = true;
                break;
            }
        }
        if (all_different) break;
    }

    /* With 5 samples and 1001 possible values, we should see variation */
    /* (This test could theoretically fail if rand() returns same value 5 times, */
    /* but probability is extremely low: 1/1001^4) */
    ck_assert(all_different);
}

END_TEST START_TEST(test_retry_delay_always_positive)
{
    /* All delays should be positive */
    ck_assert(ik_error_calc_retry_delay_ms(1, -1) > 0);
    ck_assert(ik_error_calc_retry_delay_ms(2, -1) > 0);
    ck_assert(ik_error_calc_retry_delay_ms(3, -1) > 0);
    ck_assert(ik_error_calc_retry_delay_ms(1, 0) > 0);
    ck_assert(ik_error_calc_retry_delay_ms(1, 5000) > 0);
}

END_TEST

/**
 * Test Suite Configuration
 */

static Suite *error_suite(void)
{
    Suite *s = suite_create("Provider Error Utilities");

    /* Category name tests */
    TCase *tc_category = tcase_create("Category Names");
    tcase_add_test(tc_category, test_category_name_auth);
    tcase_add_test(tc_category, test_category_name_rate_limit);
    tcase_add_test(tc_category, test_category_name_invalid_arg);
    tcase_add_test(tc_category, test_category_name_not_found);
    tcase_add_test(tc_category, test_category_name_server);
    tcase_add_test(tc_category, test_category_name_timeout);
    tcase_add_test(tc_category, test_category_name_content_filter);
    tcase_add_test(tc_category, test_category_name_network);
    tcase_add_test(tc_category, test_category_name_unknown);
    tcase_add_test(tc_category, test_category_name_invalid);
    suite_add_tcase(s, tc_category);

    /* Retryability tests */
    TCase *tc_retry = tcase_create("Retryability");
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

    /* User message tests */
    TCase *tc_message = tcase_create("User Messages");
    tcase_add_checked_fixture(tc_message, setup, teardown);
    tcase_add_test(tc_message, test_user_message_auth_anthropic);
    tcase_add_test(tc_message, test_user_message_auth_openai);
    tcase_add_test(tc_message, test_user_message_auth_google);
    tcase_add_test(tc_message, test_user_message_rate_limit_with_detail);
    tcase_add_test(tc_message, test_user_message_rate_limit_no_detail);
    tcase_add_test(tc_message, test_user_message_invalid_arg_with_detail);
    tcase_add_test(tc_message, test_user_message_invalid_arg_no_detail);
    tcase_add_test(tc_message, test_user_message_not_found_with_detail);
    tcase_add_test(tc_message, test_user_message_not_found_no_detail);
    tcase_add_test(tc_message, test_user_message_server_with_detail);
    tcase_add_test(tc_message, test_user_message_server_no_detail);
    tcase_add_test(tc_message, test_user_message_timeout);
    tcase_add_test(tc_message, test_user_message_content_filter_with_detail);
    tcase_add_test(tc_message, test_user_message_content_filter_no_detail);
    tcase_add_test(tc_message, test_user_message_network_with_detail);
    tcase_add_test(tc_message, test_user_message_network_no_detail);
    tcase_add_test(tc_message, test_user_message_unknown_with_detail);
    tcase_add_test(tc_message, test_user_message_unknown_no_detail);
    tcase_add_test(tc_message, test_user_message_empty_detail_treated_as_null);
    tcase_add_test(tc_message, test_user_message_allocated_on_context);
    suite_add_tcase(s, tc_message);

    /* Retry delay calculation tests */
    TCase *tc_delay = tcase_create("Retry Delay Calculation");
    tcase_add_test(tc_delay, test_retry_delay_provider_suggested);
    tcase_add_test(tc_delay, test_retry_delay_exponential_backoff_attempt_1);
    tcase_add_test(tc_delay, test_retry_delay_exponential_backoff_attempt_2);
    tcase_add_test(tc_delay, test_retry_delay_exponential_backoff_attempt_3);
    tcase_add_test(tc_delay, test_retry_delay_zero_triggers_backoff);
    tcase_add_test(tc_delay, test_retry_delay_negative_triggers_backoff);
    tcase_add_test(tc_delay, test_retry_delay_jitter_randomness);
    tcase_add_test(tc_delay, test_retry_delay_always_positive);
    suite_add_tcase(s, tc_delay);

    return s;
}

int main(void)
{
    Suite *s = error_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
