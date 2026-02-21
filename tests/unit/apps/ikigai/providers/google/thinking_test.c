#include "tests/test_constants.h"
/**
 * @file thinking_test.c
 * @brief Unit tests for Google thinking budget/level calculation
 */

#include <check.h>
#include <talloc.h>
#include "apps/ikigai/providers/google/thinking.h"
#include "apps/ikigai/providers/provider.h"

static TALLOC_CTX *test_ctx;

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* Model Series Detection Tests */

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
START_TEST(test_model_series_gemini_31_pro_preview) {
    ik_gemini_series_t series = ik_google_model_series("gemini-3.1-pro-preview");
    ck_assert_int_eq(series, IK_GEMINI_3);
}
END_TEST
/* Thinking Budget Calculation Tests */

START_TEST(test_thinking_budget_2_5_pro_none) {
    int32_t budget = ik_google_thinking_budget("gemini-2.5-pro", IK_THINKING_MIN);
    ck_assert_int_eq(budget, 128);
}
END_TEST
START_TEST(test_thinking_budget_2_5_pro_low) {
    int32_t budget = ik_google_thinking_budget("gemini-2.5-pro", IK_THINKING_LOW);
    ck_assert_int_eq(budget, 8192);
}
END_TEST
START_TEST(test_thinking_budget_2_5_pro_med) {
    int32_t budget = ik_google_thinking_budget("gemini-2.5-pro", IK_THINKING_MED);
    ck_assert_int_eq(budget, 16384);
}
END_TEST
START_TEST(test_thinking_budget_2_5_pro_high) {
    int32_t budget = ik_google_thinking_budget("gemini-2.5-pro", IK_THINKING_HIGH);
    ck_assert_int_eq(budget, 32768);
}
END_TEST
START_TEST(test_thinking_budget_2_5_flash_none) {
    int32_t budget = ik_google_thinking_budget("gemini-2.5-flash", IK_THINKING_MIN);
    ck_assert_int_eq(budget, 0);
}
END_TEST
START_TEST(test_thinking_budget_2_5_flash_low) {
    int32_t budget = ik_google_thinking_budget("gemini-2.5-flash", IK_THINKING_LOW);
    ck_assert_int_eq(budget, 8192);
}
END_TEST
START_TEST(test_thinking_budget_2_5_flash_med) {
    int32_t budget = ik_google_thinking_budget("gemini-2.5-flash", IK_THINKING_MED);
    ck_assert_int_eq(budget, 16384);
}
END_TEST
START_TEST(test_thinking_budget_2_5_flash_high) {
    int32_t budget = ik_google_thinking_budget("gemini-2.5-flash", IK_THINKING_HIGH);
    ck_assert_int_eq(budget, 24576);
}
END_TEST
START_TEST(test_thinking_budget_2_5_flash_lite_none) {
    int32_t budget = ik_google_thinking_budget("gemini-2.5-flash-lite", IK_THINKING_MIN);
    ck_assert_int_eq(budget, 512);
}
END_TEST
START_TEST(test_thinking_budget_2_5_flash_lite_low) {
    int32_t budget = ik_google_thinking_budget("gemini-2.5-flash-lite", IK_THINKING_LOW);
    ck_assert_int_eq(budget, 8192);
}
END_TEST
START_TEST(test_thinking_budget_2_5_flash_lite_med) {
    int32_t budget = ik_google_thinking_budget("gemini-2.5-flash-lite", IK_THINKING_MED);
    ck_assert_int_eq(budget, 16384);
}
END_TEST
START_TEST(test_thinking_budget_2_5_flash_lite_high) {
    int32_t budget = ik_google_thinking_budget("gemini-2.5-flash-lite", IK_THINKING_HIGH);
    ck_assert_int_eq(budget, 24576);
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
    // Test a Gemini 2.5 model not in BUDGET_TABLE - returns error
    int32_t budget = ik_google_thinking_budget("gemini-2.5-experimental", IK_THINKING_HIGH);
    ck_assert_int_eq(budget, -1);
}
END_TEST
START_TEST(test_thinking_budget_2_5_unknown_model_none) {
    // Test NONE level with unknown model - returns error
    int32_t budget = ik_google_thinking_budget("gemini-2.5-experimental", IK_THINKING_MIN);
    ck_assert_int_eq(budget, -1);
}
END_TEST
START_TEST(test_thinking_budget_2_5_typo_model) {
    // Test a typo in model name (flash-light instead of flash-lite) - returns error
    int32_t budget = ik_google_thinking_budget("gemini-2.5-flash-light", IK_THINKING_LOW);
    ck_assert_int_eq(budget, -1);
}
END_TEST
/* Thinking Level String Tests */

/* gemini-3-flash-preview: minimal/low/medium/high */
START_TEST(test_thinking_level_str_flash_none) { ck_assert_str_eq(ik_google_thinking_level_str("gemini-3-flash-preview", IK_THINKING_MIN), "minimal"); }
END_TEST
START_TEST(test_thinking_level_str_flash_low) { ck_assert_str_eq(ik_google_thinking_level_str("gemini-3-flash-preview", IK_THINKING_LOW), "low"); }
END_TEST
START_TEST(test_thinking_level_str_flash_med) { ck_assert_str_eq(ik_google_thinking_level_str("gemini-3-flash-preview", IK_THINKING_MED), "medium"); }
END_TEST
START_TEST(test_thinking_level_str_flash_high) { ck_assert_str_eq(ik_google_thinking_level_str("gemini-3-flash-preview", IK_THINKING_HIGH), "high"); }
END_TEST
/* gemini-3-pro-preview: low/low/high/high */
START_TEST(test_thinking_level_str_pro_none) { ck_assert_str_eq(ik_google_thinking_level_str("gemini-3-pro-preview", IK_THINKING_MIN), "low"); }
END_TEST
START_TEST(test_thinking_level_str_pro_low) { ck_assert_str_eq(ik_google_thinking_level_str("gemini-3-pro-preview", IK_THINKING_LOW), "low"); }
END_TEST
START_TEST(test_thinking_level_str_pro_med) { ck_assert_str_eq(ik_google_thinking_level_str("gemini-3-pro-preview", IK_THINKING_MED), "high"); }
END_TEST
START_TEST(test_thinking_level_str_pro_high) { ck_assert_str_eq(ik_google_thinking_level_str("gemini-3-pro-preview", IK_THINKING_HIGH), "high"); }
END_TEST
/* gemini-3.1-pro-preview: low/low/medium/high */
START_TEST(test_thinking_level_str_31_pro_none) { ck_assert_str_eq(ik_google_thinking_level_str("gemini-3.1-pro-preview", IK_THINKING_MIN), "low"); }
END_TEST
START_TEST(test_thinking_level_str_31_pro_low) { ck_assert_str_eq(ik_google_thinking_level_str("gemini-3.1-pro-preview", IK_THINKING_LOW), "low"); }
END_TEST
START_TEST(test_thinking_level_str_31_pro_med) { ck_assert_str_eq(ik_google_thinking_level_str("gemini-3.1-pro-preview", IK_THINKING_MED), "medium"); }
END_TEST
START_TEST(test_thinking_level_str_31_pro_high) { ck_assert_str_eq(ik_google_thinking_level_str("gemini-3.1-pro-preview", IK_THINKING_HIGH), "high"); }
END_TEST
/* Thinking Support Tests */

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
/* Can Disable Thinking Tests */

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
    // Test a Gemini 2.5 model not in BUDGET_TABLE - returns false (unknown)
    bool can_disable = ik_google_can_disable_thinking("gemini-2.5-experimental");
    ck_assert(!can_disable);
}
END_TEST
/* Thinking Validation Tests */

#define VT(name, model, level, expect_ok) \
START_TEST(name) { res_t r = ik_google_validate_thinking(test_ctx, model, level); ck_assert((expect_ok) ? !is_err(&r) : is_err(&r)); } \
END_TEST
VT(test_validate_thinking_2_5_flash_none, "gemini-2.5-flash", IK_THINKING_MIN, 1)
VT(test_validate_thinking_2_5_flash_low,  "gemini-2.5-flash", IK_THINKING_LOW,  1)
VT(test_validate_thinking_2_5_flash_med,  "gemini-2.5-flash", IK_THINKING_MED,  1)
VT(test_validate_thinking_2_5_flash_high, "gemini-2.5-flash", IK_THINKING_HIGH, 1)
VT(test_validate_thinking_2_5_pro_none,   "gemini-2.5-pro",   IK_THINKING_MIN, 0)
VT(test_validate_thinking_2_5_pro_low,    "gemini-2.5-pro",   IK_THINKING_LOW,  1)
VT(test_validate_thinking_2_5_pro_med,    "gemini-2.5-pro",   IK_THINKING_MED,  1)
VT(test_validate_thinking_2_5_pro_high,   "gemini-2.5-pro",   IK_THINKING_HIGH, 1)
VT(test_validate_thinking_3_pro_none,     "gemini-3-pro",     IK_THINKING_MIN, 1)
VT(test_validate_thinking_3_pro_low,      "gemini-3-pro",     IK_THINKING_LOW,  1)
VT(test_validate_thinking_3_pro_med,      "gemini-3-pro",     IK_THINKING_MED,  1)
VT(test_validate_thinking_3_pro_high,     "gemini-3-pro",     IK_THINKING_HIGH, 1)
VT(test_validate_thinking_1_5_pro_none,   "gemini-1.5-pro",   IK_THINKING_MIN, 1)
VT(test_validate_thinking_1_5_pro_low,    "gemini-1.5-pro",   IK_THINKING_LOW,  0)
START_TEST(test_validate_thinking_null_model) { res_t r = ik_google_validate_thinking(test_ctx, NULL, IK_THINKING_LOW); ck_assert(is_err(&r)); }
END_TEST

/* Test Suite Setup */

static Suite *google_thinking_suite(void)
{
    Suite *s = suite_create("Google Thinking");

    TCase *tc_series = tcase_create("Model Series Detection");
    tcase_set_timeout(tc_series, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_series, setup, teardown);
    tcase_add_test(tc_series, test_model_series_gemini_2_5_pro);
    tcase_add_test(tc_series, test_model_series_gemini_2_5_flash);
    tcase_add_test(tc_series, test_model_series_gemini_3_pro);
    tcase_add_test(tc_series, test_model_series_gemini_1_5_pro);
    tcase_add_test(tc_series, test_model_series_null);
    tcase_add_test(tc_series, test_model_series_gemini_31_pro_preview);
    suite_add_tcase(s, tc_series);

    TCase *tc_budget = tcase_create("Thinking Budget Calculation");
    tcase_set_timeout(tc_budget, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_budget, setup, teardown);
    tcase_add_test(tc_budget, test_thinking_budget_2_5_pro_none);
    tcase_add_test(tc_budget, test_thinking_budget_2_5_pro_low);
    tcase_add_test(tc_budget, test_thinking_budget_2_5_pro_med);
    tcase_add_test(tc_budget, test_thinking_budget_2_5_pro_high);
    tcase_add_test(tc_budget, test_thinking_budget_2_5_flash_none);
    tcase_add_test(tc_budget, test_thinking_budget_2_5_flash_low);
    tcase_add_test(tc_budget, test_thinking_budget_2_5_flash_med);
    tcase_add_test(tc_budget, test_thinking_budget_2_5_flash_high);
    tcase_add_test(tc_budget, test_thinking_budget_2_5_flash_lite_none);
    tcase_add_test(tc_budget, test_thinking_budget_2_5_flash_lite_low);
    tcase_add_test(tc_budget, test_thinking_budget_2_5_flash_lite_med);
    tcase_add_test(tc_budget, test_thinking_budget_2_5_flash_lite_high);
    tcase_add_test(tc_budget, test_thinking_budget_gemini_3_pro);
    tcase_add_test(tc_budget, test_thinking_budget_null);
    tcase_add_test(tc_budget, test_thinking_budget_2_5_unknown_model);
    tcase_add_test(tc_budget, test_thinking_budget_2_5_unknown_model_none);
    tcase_add_test(tc_budget, test_thinking_budget_2_5_typo_model);
    suite_add_tcase(s, tc_budget);

    TCase *tc_level = tcase_create("Thinking Level Strings");
    tcase_set_timeout(tc_level, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_level, setup, teardown);
    tcase_add_test(tc_level, test_thinking_level_str_flash_none);
    tcase_add_test(tc_level, test_thinking_level_str_flash_low);
    tcase_add_test(tc_level, test_thinking_level_str_flash_med);
    tcase_add_test(tc_level, test_thinking_level_str_flash_high);
    tcase_add_test(tc_level, test_thinking_level_str_pro_none);
    tcase_add_test(tc_level, test_thinking_level_str_pro_low);
    tcase_add_test(tc_level, test_thinking_level_str_pro_med);
    tcase_add_test(tc_level, test_thinking_level_str_pro_high);
    tcase_add_test(tc_level, test_thinking_level_str_31_pro_none);
    tcase_add_test(tc_level, test_thinking_level_str_31_pro_low);
    tcase_add_test(tc_level, test_thinking_level_str_31_pro_med);
    tcase_add_test(tc_level, test_thinking_level_str_31_pro_high);
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
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/google/thinking_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
