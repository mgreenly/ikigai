#include "../../../../src/providers/provider.h"
#include "../../../../src/providers/common/error_utils.h"
#include "../../../test_utils_helper.h"

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

/**
 * Unit tests for provider error utilities - User Messages and Retry Delays
 *
 * Tests user message generation and retry delay calculation for async event loop integration.
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
 * User Message Tests
 */

START_TEST(test_user_message_auth_anthropic) {
    char *msg = ik_error_user_message(test_ctx, "anthropic", IK_ERR_CAT_AUTH, NULL);
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg,
                     "Authentication failed for anthropic. Check your API key in ANTHROPIC_API_KEY or ~/.config/ikigai/credentials.json");
    ck_assert(talloc_get_size(msg) > 0); /* Verify allocated on context */
}

END_TEST

START_TEST(test_user_message_auth_openai) {
    char *msg = ik_error_user_message(test_ctx, "openai", IK_ERR_CAT_AUTH, NULL);
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg,
                     "Authentication failed for openai. Check your API key in OPENAI_API_KEY or ~/.config/ikigai/credentials.json");
}

END_TEST

START_TEST(test_user_message_auth_google) {
    char *msg = ik_error_user_message(test_ctx, "google", IK_ERR_CAT_AUTH, NULL);
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg,
                     "Authentication failed for google. Check your API key in GOOGLE_API_KEY or ~/.config/ikigai/credentials.json");
}

END_TEST

START_TEST(test_user_message_rate_limit_with_detail) {
    char *msg = ik_error_user_message(test_ctx, "anthropic", IK_ERR_CAT_RATE_LIMIT, "Try again in 60 seconds");
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "Rate limit exceeded for anthropic. Try again in 60 seconds");
}

END_TEST

START_TEST(test_user_message_rate_limit_no_detail) {
    char *msg = ik_error_user_message(test_ctx, "anthropic", IK_ERR_CAT_RATE_LIMIT, NULL);
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "Rate limit exceeded for anthropic.");
}

END_TEST

START_TEST(test_user_message_invalid_arg_with_detail) {
    char *msg = ik_error_user_message(test_ctx, "openai", IK_ERR_CAT_INVALID_ARG, "max_tokens must be positive");
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "Invalid request to openai: max_tokens must be positive");
}

END_TEST

START_TEST(test_user_message_invalid_arg_no_detail) {
    char *msg = ik_error_user_message(test_ctx, "openai", IK_ERR_CAT_INVALID_ARG, NULL);
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "Invalid request to openai");
}

END_TEST

START_TEST(test_user_message_not_found_with_detail) {
    char *msg = ik_error_user_message(test_ctx, "google", IK_ERR_CAT_NOT_FOUND, "gemini-99 does not exist");
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "Model not found on google: gemini-99 does not exist");
}

END_TEST

START_TEST(test_user_message_not_found_no_detail) {
    char *msg = ik_error_user_message(test_ctx, "google", IK_ERR_CAT_NOT_FOUND, NULL);
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "Model not found on google");
}

END_TEST

START_TEST(test_user_message_server_with_detail) {
    char *msg = ik_error_user_message(test_ctx, "anthropic", IK_ERR_CAT_SERVER, "Overloaded");
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "anthropic server error. This is temporary, retrying may succeed. Overloaded");
}

END_TEST

START_TEST(test_user_message_server_no_detail) {
    char *msg = ik_error_user_message(test_ctx, "anthropic", IK_ERR_CAT_SERVER, NULL);
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "anthropic server error. This is temporary, retrying may succeed.");
}

END_TEST

START_TEST(test_user_message_timeout) {
    char *msg = ik_error_user_message(test_ctx, "openai", IK_ERR_CAT_TIMEOUT, NULL);
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "Request to openai timed out. Check network connection.");
}

END_TEST

START_TEST(test_user_message_content_filter_with_detail) {
    char *msg = ik_error_user_message(test_ctx, "google", IK_ERR_CAT_CONTENT_FILTER, "Harmful content detected");
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "Content blocked by google safety filters: Harmful content detected");
}

END_TEST

START_TEST(test_user_message_content_filter_no_detail) {
    char *msg = ik_error_user_message(test_ctx, "google", IK_ERR_CAT_CONTENT_FILTER, NULL);
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "Content blocked by google safety filters");
}

END_TEST

START_TEST(test_user_message_network_with_detail) {
    char *msg = ik_error_user_message(test_ctx, "anthropic", IK_ERR_CAT_NETWORK, "Connection refused");
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "Network error connecting to anthropic: Connection refused");
}

END_TEST

START_TEST(test_user_message_network_no_detail) {
    char *msg = ik_error_user_message(test_ctx, "anthropic", IK_ERR_CAT_NETWORK, NULL);
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "Network error connecting to anthropic");
}

END_TEST

START_TEST(test_user_message_unknown_with_detail) {
    char *msg = ik_error_user_message(test_ctx, "openai", IK_ERR_CAT_UNKNOWN, "Something went wrong");
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "openai error: Something went wrong");
}

END_TEST

START_TEST(test_user_message_unknown_no_detail) {
    char *msg = ik_error_user_message(test_ctx, "openai", IK_ERR_CAT_UNKNOWN, NULL);
    ck_assert_ptr_nonnull(msg);
    ck_assert_str_eq(msg, "openai error");
}

END_TEST

START_TEST(test_user_message_empty_detail_treated_as_null) {
    char *msg = ik_error_user_message(test_ctx, "anthropic", IK_ERR_CAT_RATE_LIMIT, "");
    ck_assert_ptr_nonnull(msg);
    /* Empty string should be treated same as NULL - no trailing detail */
    ck_assert_str_eq(msg, "Rate limit exceeded for anthropic.");
}

END_TEST

START_TEST(test_user_message_allocated_on_context) {
    char *msg = ik_error_user_message(test_ctx, "openai", IK_ERR_CAT_SERVER, "Test");
    ck_assert_ptr_nonnull(msg);
    /* Verify message is allocated (has non-zero size) */
    size_t size = talloc_get_size(msg);
    ck_assert(size > 0);
}

END_TEST

START_TEST(test_user_message_google_provider_multiple_categories) {
    /* Test google provider with multiple error categories to ensure branch coverage */
    char *msg;

    /* Auth error with google */
    msg = ik_error_user_message(test_ctx, "google", IK_ERR_CAT_AUTH, NULL);
    ck_assert_ptr_nonnull(msg);
    ck_assert(strstr(msg, "GOOGLE_API_KEY") != NULL);

    /* Server error with google */
    msg = ik_error_user_message(test_ctx, "google", IK_ERR_CAT_SERVER, NULL);
    ck_assert_ptr_nonnull(msg);
    ck_assert(strstr(msg, "google") != NULL);

    /* Rate limit with google */
    msg = ik_error_user_message(test_ctx, "google", IK_ERR_CAT_RATE_LIMIT, NULL);
    ck_assert_ptr_nonnull(msg);
    ck_assert(strstr(msg, "google") != NULL);
}

END_TEST
/**
 * Retry Delay Calculation Tests
 */

/**
 * Test Suite Configuration
 */

static Suite *error_messages_suite(void)
{
    Suite *s = suite_create("Provider Error Messages and Delays");

    /* User message tests */
    TCase *tc_message = tcase_create("User Messages");
    tcase_set_timeout(tc_message, IK_TEST_TIMEOUT);
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
    tcase_add_test(tc_message, test_user_message_google_provider_multiple_categories);
    suite_add_tcase(s, tc_message);

    return s;
}

int main(void)
{
    Suite *s = error_messages_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int32_t number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
