#include "../../../test_constants.h"
/**
 * @file thinking_test.c
 * @brief Unit tests for Google thinking budget/level calculation
 */

#include <check.h>
#include <talloc.h>
#include "providers/google/thinking.h"
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
 * Model Series Detection Tests
 * ================================================================ */

START_TEST(test_model_series_gemini_2_5_pro) {
    ik_gemini_series_t series = ik_google_model_series("gemini-2.5-pro");
    ck_assert_int_eq(series, IK_GEMINI_2_5);
}
END_TEST

START_TEST(test_model_series_gemini_2_5_flash) {
    ik_gemini_series_t series = ik_google_model_series("gemini-2.5-flash");
    ck_assert_int_eq(series, IK_GEMINI_2_5);
}

END_TEST

START_TEST(test_model_series_gemini_2_0_flash) {
    ik_gemini_series_t series = ik_google_model_series("gemini-2.0-flash");
    ck_assert_int_eq(series, IK_GEMINI_2_5);
}

END_TEST

START_TEST(test_model_series_gemini_3_pro) {
    ik_gemini_series_t series = ik_google_model_series("gemini-3-pro");
    ck_assert_int_eq(series, IK_GEMINI_3);
}

END_TEST

START_TEST(test_model_series_gemini_1_5_pro) {
    ik_gemini_series_t series = ik_google_model_series("gemini-1.5-pro");
    ck_assert_int_eq(series, IK_GEMINI_OTHER);
}

END_TEST

START_TEST(test_model_series_null) {
    ik_gemini_series_t series = ik_google_model_series(NULL);
    ck_assert_int_eq(series, IK_GEMINI_OTHER);
}

END_TEST
/* ================================================================
 * Thinking Budget Calculation Tests
 * ================================================================ */

START_TEST(test_thinking_budget_2_5_pro_none) {
    int32_t budget = ik_google_thinking_budget("gemini-2.5-pro", IK_THINKING_NONE);
    ck_assert_int_eq(budget, 128); // minimum
}

END_TEST

START_TEST(test_thinking_budget_2_5_pro_low) {
    int32_t budget = ik_google_thinking_budget("gemini-2.5-pro", IK_THINKING_LOW);
    // min=128, max=32768, range=32640
    // LOW = 128 + 32640/3 = 128 + 10880 = 11008
    ck_assert_int_eq(budget, 11008);
}

END_TEST

START_TEST(test_thinking_budget_2_5_pro_med) {
    int32_t budget = ik_google_thinking_budget("gemini-2.5-pro", IK_THINKING_MED);
    // min=128, max=32768, range=32640
    // MED = 128 + 2*32640/3 = 128 + 21760 = 21888
    ck_assert_int_eq(budget, 21888);
}

END_TEST

START_TEST(test_thinking_budget_2_5_pro_high) {
    int32_t budget = ik_google_thinking_budget("gemini-2.5-pro", IK_THINKING_HIGH);
    ck_assert_int_eq(budget, 32768); // maximum
}

END_TEST

START_TEST(test_thinking_budget_2_5_flash_none) {
    int32_t budget = ik_google_thinking_budget("gemini-2.5-flash", IK_THINKING_NONE);
    ck_assert_int_eq(budget, 0); // can disable
}

END_TEST

START_TEST(test_thinking_budget_2_5_flash_med) {
    int32_t budget = ik_google_thinking_budget("gemini-2.5-flash", IK_THINKING_MED);
    // min=0, max=24576, range=24576
    // MED = 0 + 2*24576/3 = 16384
    ck_assert_int_eq(budget, 16384);
}

END_TEST

START_TEST(test_thinking_budget_gemini_3_pro) {
    int32_t budget = ik_google_thinking_budget("gemini-3-pro", IK_THINKING_HIGH);
    ck_assert_int_eq(budget, -1); // uses levels not budgets
}

END_TEST

START_TEST(test_thinking_budget_null) {
    int32_t budget = ik_google_thinking_budget(NULL, IK_THINKING_HIGH);
    ck_assert_int_eq(budget, -1);
}

END_TEST

START_TEST(test_thinking_budget_2_5_unknown_model) {
    // Test a Gemini 2.5 model not in BUDGET_TABLE - uses defaults
    int32_t budget = ik_google_thinking_budget("gemini-2.5-experimental", IK_THINKING_HIGH);
    ck_assert_int_eq(budget, 24576); // DEFAULT_MAX_BUDGET
}

END_TEST

START_TEST(test_thinking_budget_2_5_unknown_model_none) {
    // Test NONE level with unknown model - uses DEFAULT_MIN_BUDGET
    int32_t budget = ik_google_thinking_budget("gemini-2.5-experimental", IK_THINKING_NONE);
    ck_assert_int_eq(budget, 0); // DEFAULT_MIN_BUDGET
}

END_TEST
/* ================================================================
 * Thinking Level String Tests
 * ================================================================ */

START_TEST(test_thinking_level_str_none) {
    const char *level = ik_google_thinking_level_str(IK_THINKING_NONE);
    ck_assert_ptr_null(level);
}

END_TEST

START_TEST(test_thinking_level_str_low) {
    const char *level = ik_google_thinking_level_str(IK_THINKING_LOW);
    ck_assert_str_eq(level, "LOW");
}

END_TEST

START_TEST(test_thinking_level_str_med) {
    const char *level = ik_google_thinking_level_str(IK_THINKING_MED);
    ck_assert_str_eq(level, "LOW"); // maps to LOW
}

END_TEST

START_TEST(test_thinking_level_str_high) {
    const char *level = ik_google_thinking_level_str(IK_THINKING_HIGH);
    ck_assert_str_eq(level, "HIGH");
}

END_TEST
/* ================================================================
 * Thinking Support Tests
 * ================================================================ */

START_TEST(test_supports_thinking_2_5_pro) {
    bool supports = ik_google_supports_thinking("gemini-2.5-pro");
    ck_assert(supports);
}

END_TEST

START_TEST(test_supports_thinking_3_pro) {
    bool supports = ik_google_supports_thinking("gemini-3-pro");
    ck_assert(supports);
}

END_TEST

START_TEST(test_supports_thinking_1_5_pro) {
    bool supports = ik_google_supports_thinking("gemini-1.5-pro");
    ck_assert(!supports);
}

END_TEST

START_TEST(test_supports_thinking_null) {
    bool supports = ik_google_supports_thinking(NULL);
    ck_assert(!supports);
}

END_TEST
/* ================================================================
 * Can Disable Thinking Tests
 * ================================================================ */

START_TEST(test_can_disable_thinking_2_5_pro) {
    bool can_disable = ik_google_can_disable_thinking("gemini-2.5-pro");
    ck_assert(!can_disable); // min=128
}

END_TEST

START_TEST(test_can_disable_thinking_2_5_flash) {
    bool can_disable = ik_google_can_disable_thinking("gemini-2.5-flash");
    ck_assert(can_disable); // min=0
}

END_TEST

START_TEST(test_can_disable_thinking_2_5_flash_lite) {
    bool can_disable = ik_google_can_disable_thinking("gemini-2.5-flash-lite");
    ck_assert(!can_disable); // min=512
}

END_TEST

START_TEST(test_can_disable_thinking_3_pro) {
    bool can_disable = ik_google_can_disable_thinking("gemini-3-pro");
    ck_assert(!can_disable); // uses levels
}

END_TEST

START_TEST(test_can_disable_thinking_null) {
    bool can_disable = ik_google_can_disable_thinking(NULL);
    ck_assert(!can_disable);
}

END_TEST

START_TEST(test_can_disable_thinking_1_5_pro) {
    bool can_disable = ik_google_can_disable_thinking("gemini-1.5-pro");
    ck_assert(!can_disable); // doesn't support thinking
}

END_TEST

START_TEST(test_can_disable_thinking_2_5_unknown) {
    // Test a Gemini 2.5 model not in BUDGET_TABLE - uses defaults
    bool can_disable = ik_google_can_disable_thinking("gemini-2.5-experimental");
    ck_assert(can_disable); // DEFAULT_MIN_BUDGET = 0
}

END_TEST
/* ================================================================
 * Thinking Validation Tests
 * ================================================================ */

START_TEST(test_validate_thinking_2_5_flash_none) {
    res_t result = ik_google_validate_thinking(test_ctx, "gemini-2.5-flash", IK_THINKING_NONE);
    ck_assert(!is_err(&result)); // OK - can disable
}

END_TEST

START_TEST(test_validate_thinking_2_5_flash_low) {
    res_t result = ik_google_validate_thinking(test_ctx, "gemini-2.5-flash", IK_THINKING_LOW);
    ck_assert(!is_err(&result)); // OK
}

END_TEST

START_TEST(test_validate_thinking_2_5_flash_med) {
    res_t result = ik_google_validate_thinking(test_ctx, "gemini-2.5-flash", IK_THINKING_MED);
    ck_assert(!is_err(&result)); // OK
}

END_TEST

START_TEST(test_validate_thinking_2_5_flash_high) {
    res_t result = ik_google_validate_thinking(test_ctx, "gemini-2.5-flash", IK_THINKING_HIGH);
    ck_assert(!is_err(&result)); // OK
}

END_TEST

START_TEST(test_validate_thinking_2_5_pro_none) {
    res_t result = ik_google_validate_thinking(test_ctx, "gemini-2.5-pro", IK_THINKING_NONE);
    ck_assert(is_err(&result)); // ERR - cannot disable (min=128)
}

END_TEST

START_TEST(test_validate_thinking_2_5_pro_low) {
    res_t result = ik_google_validate_thinking(test_ctx, "gemini-2.5-pro", IK_THINKING_LOW);
    ck_assert(!is_err(&result)); // OK
}

END_TEST

START_TEST(test_validate_thinking_2_5_pro_med) {
    res_t result = ik_google_validate_thinking(test_ctx, "gemini-2.5-pro", IK_THINKING_MED);
    ck_assert(!is_err(&result)); // OK
}

END_TEST

START_TEST(test_validate_thinking_2_5_pro_high) {
    res_t result = ik_google_validate_thinking(test_ctx, "gemini-2.5-pro", IK_THINKING_HIGH);
    ck_assert(!is_err(&result)); // OK
}

END_TEST

START_TEST(test_validate_thinking_3_pro_none) {
    res_t result = ik_google_validate_thinking(test_ctx, "gemini-3-pro", IK_THINKING_NONE);
    ck_assert(!is_err(&result)); // OK - NONE means don't send thinking config
}

END_TEST

START_TEST(test_validate_thinking_3_pro_low) {
    res_t result = ik_google_validate_thinking(test_ctx, "gemini-3-pro", IK_THINKING_LOW);
    ck_assert(!is_err(&result)); // OK
}

END_TEST

START_TEST(test_validate_thinking_3_pro_med) {
    res_t result = ik_google_validate_thinking(test_ctx, "gemini-3-pro", IK_THINKING_MED);
    ck_assert(!is_err(&result)); // OK
}

END_TEST

START_TEST(test_validate_thinking_3_pro_high) {
    res_t result = ik_google_validate_thinking(test_ctx, "gemini-3-pro", IK_THINKING_HIGH);
    ck_assert(!is_err(&result)); // OK
}

END_TEST

START_TEST(test_validate_thinking_1_5_pro_none) {
    res_t result = ik_google_validate_thinking(test_ctx, "gemini-1.5-pro", IK_THINKING_NONE);
    ck_assert(!is_err(&result)); // OK - NONE is always valid
}

END_TEST

START_TEST(test_validate_thinking_1_5_pro_low) {
    res_t result = ik_google_validate_thinking(test_ctx, "gemini-1.5-pro", IK_THINKING_LOW);
    ck_assert(is_err(&result)); // ERR - doesn't support thinking
}

END_TEST

START_TEST(test_validate_thinking_null_model) {
    res_t result = ik_google_validate_thinking(test_ctx, NULL, IK_THINKING_LOW);
    ck_assert(is_err(&result)); // ERR(INVALID_ARG)
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_thinking_suite(void)
{
    Suite *s = suite_create("Google Thinking");

    TCase *tc_series = tcase_create("Model Series Detection");
    tcase_set_timeout(tc_series, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_series, setup, teardown);
    tcase_add_test(tc_series, test_model_series_gemini_2_5_pro);
    tcase_add_test(tc_series, test_model_series_gemini_2_5_flash);
    tcase_add_test(tc_series, test_model_series_gemini_2_0_flash);
    tcase_add_test(tc_series, test_model_series_gemini_3_pro);
    tcase_add_test(tc_series, test_model_series_gemini_1_5_pro);
    tcase_add_test(tc_series, test_model_series_null);
    suite_add_tcase(s, tc_series);

    TCase *tc_budget = tcase_create("Thinking Budget Calculation");
    tcase_set_timeout(tc_budget, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_budget, setup, teardown);
    tcase_add_test(tc_budget, test_thinking_budget_2_5_pro_none);
    tcase_add_test(tc_budget, test_thinking_budget_2_5_pro_low);
    tcase_add_test(tc_budget, test_thinking_budget_2_5_pro_med);
    tcase_add_test(tc_budget, test_thinking_budget_2_5_pro_high);
    tcase_add_test(tc_budget, test_thinking_budget_2_5_flash_none);
    tcase_add_test(tc_budget, test_thinking_budget_2_5_flash_med);
    tcase_add_test(tc_budget, test_thinking_budget_gemini_3_pro);
    tcase_add_test(tc_budget, test_thinking_budget_null);
    tcase_add_test(tc_budget, test_thinking_budget_2_5_unknown_model);
    tcase_add_test(tc_budget, test_thinking_budget_2_5_unknown_model_none);
    suite_add_tcase(s, tc_budget);

    TCase *tc_level = tcase_create("Thinking Level Strings");
    tcase_set_timeout(tc_level, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_level, setup, teardown);
    tcase_add_test(tc_level, test_thinking_level_str_none);
    tcase_add_test(tc_level, test_thinking_level_str_low);
    tcase_add_test(tc_level, test_thinking_level_str_med);
    tcase_add_test(tc_level, test_thinking_level_str_high);
    suite_add_tcase(s, tc_level);

    TCase *tc_support = tcase_create("Thinking Support");
    tcase_set_timeout(tc_support, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_support, setup, teardown);
    tcase_add_test(tc_support, test_supports_thinking_2_5_pro);
    tcase_add_test(tc_support, test_supports_thinking_3_pro);
    tcase_add_test(tc_support, test_supports_thinking_1_5_pro);
    tcase_add_test(tc_support, test_supports_thinking_null);
    suite_add_tcase(s, tc_support);

    TCase *tc_disable = tcase_create("Can Disable Thinking");
    tcase_set_timeout(tc_disable, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_disable, setup, teardown);
    tcase_add_test(tc_disable, test_can_disable_thinking_2_5_pro);
    tcase_add_test(tc_disable, test_can_disable_thinking_2_5_flash);
    tcase_add_test(tc_disable, test_can_disable_thinking_2_5_flash_lite);
    tcase_add_test(tc_disable, test_can_disable_thinking_3_pro);
    tcase_add_test(tc_disable, test_can_disable_thinking_null);
    tcase_add_test(tc_disable, test_can_disable_thinking_1_5_pro);
    tcase_add_test(tc_disable, test_can_disable_thinking_2_5_unknown);
    suite_add_tcase(s, tc_disable);

    TCase *tc_validate = tcase_create("Thinking Validation");
    tcase_set_timeout(tc_validate, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_validate, setup, teardown);
    tcase_add_test(tc_validate, test_validate_thinking_2_5_flash_none);
    tcase_add_test(tc_validate, test_validate_thinking_2_5_flash_low);
    tcase_add_test(tc_validate, test_validate_thinking_2_5_flash_med);
    tcase_add_test(tc_validate, test_validate_thinking_2_5_flash_high);
    tcase_add_test(tc_validate, test_validate_thinking_2_5_pro_none);
    tcase_add_test(tc_validate, test_validate_thinking_2_5_pro_low);
    tcase_add_test(tc_validate, test_validate_thinking_2_5_pro_med);
    tcase_add_test(tc_validate, test_validate_thinking_2_5_pro_high);
    tcase_add_test(tc_validate, test_validate_thinking_3_pro_none);
    tcase_add_test(tc_validate, test_validate_thinking_3_pro_low);
    tcase_add_test(tc_validate, test_validate_thinking_3_pro_med);
    tcase_add_test(tc_validate, test_validate_thinking_3_pro_high);
    tcase_add_test(tc_validate, test_validate_thinking_1_5_pro_none);
    tcase_add_test(tc_validate, test_validate_thinking_1_5_pro_low);
    tcase_add_test(tc_validate, test_validate_thinking_null_model);
    suite_add_tcase(s, tc_validate);

    return s;
}

int main(void)
{
    Suite *s = google_thinking_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
