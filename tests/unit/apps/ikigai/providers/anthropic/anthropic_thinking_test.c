#include "tests/test_constants.h"
/**
 * @file anthropic_thinking_test.c
 * @brief Unit tests for Anthropic thinking budget/level calculation
 */

#include <check.h>
#include <talloc.h>
#include "apps/ikigai/providers/anthropic/thinking.h"
#include "apps/ikigai/providers/provider.h"
#include "shared/error.h"

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
 * Thinking Support Tests
 * ================================================================ */

START_TEST(test_supports_thinking_sonnet_4_5) {
    bool supports = ik_anthropic_supports_thinking("claude-sonnet-4-5");
    ck_assert(supports);
}
END_TEST

START_TEST(test_supports_thinking_haiku_4_5) {
    bool supports = ik_anthropic_supports_thinking("claude-haiku-4-5");
    ck_assert(supports);
}

END_TEST

START_TEST(test_supports_thinking_opus) {
    bool supports = ik_anthropic_supports_thinking("claude-opus-3-5");
    ck_assert(supports);
}

END_TEST

START_TEST(test_supports_thinking_non_claude) {
    bool supports = ik_anthropic_supports_thinking("gpt-4");
    ck_assert(!supports);
}

END_TEST

START_TEST(test_supports_thinking_null) {
    bool supports = ik_anthropic_supports_thinking(NULL);
    ck_assert(!supports);
}

END_TEST
/* ================================================================
 * Thinking Budget Calculation Tests - Sonnet 4.5
 * ================================================================ */

START_TEST(test_thinking_budget_sonnet_none) {
    int32_t budget = ik_anthropic_thinking_budget("claude-sonnet-4-5", IK_THINKING_NONE);
    ck_assert_int_eq(budget, 1024); // minimum
}

END_TEST

START_TEST(test_thinking_budget_sonnet_low) {
    int32_t budget = ik_anthropic_thinking_budget("claude-sonnet-4-5", IK_THINKING_LOW);
    // min=1024, max=65536, range=64512
    // LOW = 1024 + 64512/3 = 22528 -> floor to 2^14 = 16384
    ck_assert_int_eq(budget, 16384);
}

END_TEST

START_TEST(test_thinking_budget_sonnet_med) {
    int32_t budget = ik_anthropic_thinking_budget("claude-sonnet-4-5", IK_THINKING_MED);
    // min=1024, max=65536, range=64512
    // MED = 1024 + 2*64512/3 = 44032 -> floor to 2^15 = 32768
    ck_assert_int_eq(budget, 32768);
}

END_TEST

START_TEST(test_thinking_budget_sonnet_high) {
    int32_t budget = ik_anthropic_thinking_budget("claude-sonnet-4-5", IK_THINKING_HIGH);
    ck_assert_int_eq(budget, 65536); // max (64*1024)
}

END_TEST
/* ================================================================
 * Thinking Budget Calculation Tests - Haiku 4.5
 * ================================================================ */

START_TEST(test_thinking_budget_haiku_none) {
    int32_t budget = ik_anthropic_thinking_budget("claude-haiku-4-5", IK_THINKING_NONE);
    ck_assert_int_eq(budget, 1024); // minimum
}

END_TEST

START_TEST(test_thinking_budget_haiku_low) {
    int32_t budget = ik_anthropic_thinking_budget("claude-haiku-4-5", IK_THINKING_LOW);
    // min=1024, max=32768, range=31744
    // LOW = 1024 + 31744/3 = 11605 -> floor to 2^13 = 8192
    ck_assert_int_eq(budget, 8192);
}

END_TEST

START_TEST(test_thinking_budget_haiku_med) {
    int32_t budget = ik_anthropic_thinking_budget("claude-haiku-4-5", IK_THINKING_MED);
    // min=1024, max=32768, range=31744
    // MED = 1024 + 2*31744/3 = 22186 -> floor to 2^14 = 16384
    ck_assert_int_eq(budget, 16384);
}

END_TEST

START_TEST(test_thinking_budget_haiku_high) {
    int32_t budget = ik_anthropic_thinking_budget("claude-haiku-4-5", IK_THINKING_HIGH);
    ck_assert_int_eq(budget, 32768); // max (32*1024)
}

END_TEST
/* ================================================================
 * Thinking Budget Calculation Tests - Unknown Claude Model
 * ================================================================ */

START_TEST(test_thinking_budget_unknown_claude_none) {
    int32_t budget = ik_anthropic_thinking_budget("claude-unknown-model", IK_THINKING_NONE);
    ck_assert_int_eq(budget, 1024); // default minimum
}

END_TEST

START_TEST(test_thinking_budget_unknown_claude_low) {
    int32_t budget = ik_anthropic_thinking_budget("claude-unknown-model", IK_THINKING_LOW);
    // min=1024, max=32768, range=31744
    // LOW = 1024 + 31744/3 = 11605 -> floor to 2^13 = 8192
    ck_assert_int_eq(budget, 8192);
}

END_TEST

START_TEST(test_thinking_budget_unknown_claude_med) {
    int32_t budget = ik_anthropic_thinking_budget("claude-unknown-model", IK_THINKING_MED);
    // min=1024, max=32768, range=31744
    // MED = 1024 + 2*31744/3 = 22186 -> floor to 2^14 = 16384
    ck_assert_int_eq(budget, 16384);
}

END_TEST

START_TEST(test_thinking_budget_unknown_claude_high) {
    int32_t budget = ik_anthropic_thinking_budget("claude-unknown-model", IK_THINKING_HIGH);
    ck_assert_int_eq(budget, 32768); // default max (32*1024)
}

END_TEST
/* ================================================================
 * Thinking Budget Calculation Tests - Non-Claude Models
 * ================================================================ */

START_TEST(test_thinking_budget_non_claude) {
    int32_t budget = ik_anthropic_thinking_budget("gpt-4", IK_THINKING_LOW);
    ck_assert_int_eq(budget, -1); // unsupported
}

END_TEST

START_TEST(test_thinking_budget_null_model) {
    int32_t budget = ik_anthropic_thinking_budget(NULL, IK_THINKING_LOW);
    ck_assert_int_eq(budget, -1); // unsupported
}

END_TEST
/* ================================================================
 * Adaptive Thinking Tests - Model Detection
 * ================================================================ */

START_TEST(test_is_adaptive_model_opus_4_6) {
    bool is_adaptive = ik_anthropic_is_adaptive_model("claude-opus-4-6");
    ck_assert(is_adaptive);
}

END_TEST

START_TEST(test_is_adaptive_model_sonnet_4_6) {
    bool is_adaptive = ik_anthropic_is_adaptive_model("claude-sonnet-4-6");
    ck_assert(is_adaptive);
}

END_TEST

START_TEST(test_is_adaptive_model_sonnet_4_5) {
    bool is_adaptive = ik_anthropic_is_adaptive_model("claude-sonnet-4-5");
    ck_assert(!is_adaptive);
}

END_TEST

START_TEST(test_is_adaptive_model_haiku_4_5) {
    bool is_adaptive = ik_anthropic_is_adaptive_model("claude-haiku-4-5");
    ck_assert(!is_adaptive);
}

END_TEST

START_TEST(test_is_adaptive_model_opus_4_5) {
    bool is_adaptive = ik_anthropic_is_adaptive_model("claude-opus-4-5");
    ck_assert(!is_adaptive);
}

END_TEST

START_TEST(test_is_adaptive_model_null) {
    bool is_adaptive = ik_anthropic_is_adaptive_model(NULL);
    ck_assert(!is_adaptive);
}

END_TEST

/* ================================================================
 * Adaptive Thinking Tests - Effort Mapping
 * ================================================================ */

START_TEST(test_thinking_effort_opus_4_6_none) {
    const char *effort = ik_anthropic_thinking_effort("claude-opus-4-6", IK_THINKING_NONE);
    ck_assert_ptr_null(effort); // NONE -> omit thinking
}

END_TEST

START_TEST(test_thinking_effort_opus_4_6_low) {
    const char *effort = ik_anthropic_thinking_effort("claude-opus-4-6", IK_THINKING_LOW);
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(effort, "low");
}

END_TEST

START_TEST(test_thinking_effort_opus_4_6_med) {
    const char *effort = ik_anthropic_thinking_effort("claude-opus-4-6", IK_THINKING_MED);
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(effort, "medium");
}

END_TEST

START_TEST(test_thinking_effort_opus_4_6_high) {
    const char *effort = ik_anthropic_thinking_effort("claude-opus-4-6", IK_THINKING_HIGH);
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(effort, "high");
}

END_TEST

START_TEST(test_thinking_effort_sonnet_4_6_none) {
    const char *effort = ik_anthropic_thinking_effort("claude-sonnet-4-6", IK_THINKING_NONE);
    ck_assert_ptr_null(effort); // NONE -> omit thinking
}

END_TEST

START_TEST(test_thinking_effort_sonnet_4_6_low) {
    const char *effort = ik_anthropic_thinking_effort("claude-sonnet-4-6", IK_THINKING_LOW);
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(effort, "low");
}

END_TEST

START_TEST(test_thinking_effort_sonnet_4_6_med) {
    const char *effort = ik_anthropic_thinking_effort("claude-sonnet-4-6", IK_THINKING_MED);
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(effort, "medium");
}

END_TEST

START_TEST(test_thinking_effort_sonnet_4_6_high) {
    const char *effort = ik_anthropic_thinking_effort("claude-sonnet-4-6", IK_THINKING_HIGH);
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(effort, "high");
}

END_TEST

START_TEST(test_thinking_effort_non_adaptive_model) {
    const char *effort = ik_anthropic_thinking_effort("claude-sonnet-4-5", IK_THINKING_LOW);
    ck_assert_ptr_null(effort); // Budget-based models return NULL
}

END_TEST

START_TEST(test_thinking_effort_null_model) {
    const char *effort = ik_anthropic_thinking_effort(NULL, IK_THINKING_LOW);
    ck_assert_ptr_null(effort);
}

END_TEST

/* ================================================================
 * Thinking Budget Calculation Tests - Opus 4.5
 * ================================================================ */

START_TEST(test_thinking_budget_opus_4_5_none) {
    int32_t budget = ik_anthropic_thinking_budget("claude-opus-4-5", IK_THINKING_NONE);
    ck_assert_int_eq(budget, 1024); // minimum
}

END_TEST

START_TEST(test_thinking_budget_opus_4_5_low) {
    int32_t budget = ik_anthropic_thinking_budget("claude-opus-4-5", IK_THINKING_LOW);
    // min=1024, max=65536, range=64512
    // LOW = 1024 + 64512/3 = 22528 -> floor to 2^14 = 16384
    ck_assert_int_eq(budget, 16384);
}

END_TEST

START_TEST(test_thinking_budget_opus_4_5_med) {
    int32_t budget = ik_anthropic_thinking_budget("claude-opus-4-5", IK_THINKING_MED);
    // min=1024, max=65536, range=64512
    // MED = 1024 + 2*64512/3 = 44032 -> floor to 2^15 = 32768
    ck_assert_int_eq(budget, 32768);
}

END_TEST

START_TEST(test_thinking_budget_opus_4_5_high) {
    int32_t budget = ik_anthropic_thinking_budget("claude-opus-4-5", IK_THINKING_HIGH);
    ck_assert_int_eq(budget, 65536); // max
}

END_TEST

/* ================================================================
 * Thinking Validation Tests
 * ================================================================ */

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *anthropic_thinking_suite(void)
{
    Suite *s = suite_create("Anthropic Thinking");

    TCase *tc_support = tcase_create("Thinking Support");
    tcase_set_timeout(tc_support, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_support, setup, teardown);
    tcase_add_test(tc_support, test_supports_thinking_sonnet_4_5);
    tcase_add_test(tc_support, test_supports_thinking_haiku_4_5);
    tcase_add_test(tc_support, test_supports_thinking_opus);
    tcase_add_test(tc_support, test_supports_thinking_non_claude);
    tcase_add_test(tc_support, test_supports_thinking_null);
    suite_add_tcase(s, tc_support);

    TCase *tc_budget_sonnet = tcase_create("Thinking Budget - Sonnet 4.5");
    tcase_set_timeout(tc_budget_sonnet, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_budget_sonnet, setup, teardown);
    tcase_add_test(tc_budget_sonnet, test_thinking_budget_sonnet_none);
    tcase_add_test(tc_budget_sonnet, test_thinking_budget_sonnet_low);
    tcase_add_test(tc_budget_sonnet, test_thinking_budget_sonnet_med);
    tcase_add_test(tc_budget_sonnet, test_thinking_budget_sonnet_high);
    suite_add_tcase(s, tc_budget_sonnet);

    TCase *tc_budget_haiku = tcase_create("Thinking Budget - Haiku 4.5");
    tcase_set_timeout(tc_budget_haiku, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_budget_haiku, setup, teardown);
    tcase_add_test(tc_budget_haiku, test_thinking_budget_haiku_none);
    tcase_add_test(tc_budget_haiku, test_thinking_budget_haiku_low);
    tcase_add_test(tc_budget_haiku, test_thinking_budget_haiku_med);
    tcase_add_test(tc_budget_haiku, test_thinking_budget_haiku_high);
    suite_add_tcase(s, tc_budget_haiku);

    TCase *tc_budget_unknown = tcase_create("Thinking Budget - Unknown Claude");
    tcase_set_timeout(tc_budget_unknown, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_budget_unknown, setup, teardown);
    tcase_add_test(tc_budget_unknown, test_thinking_budget_unknown_claude_none);
    tcase_add_test(tc_budget_unknown, test_thinking_budget_unknown_claude_low);
    tcase_add_test(tc_budget_unknown, test_thinking_budget_unknown_claude_med);
    tcase_add_test(tc_budget_unknown, test_thinking_budget_unknown_claude_high);
    suite_add_tcase(s, tc_budget_unknown);

    TCase *tc_budget_non_claude = tcase_create("Thinking Budget - Non-Claude");
    tcase_set_timeout(tc_budget_non_claude, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_budget_non_claude, setup, teardown);
    tcase_add_test(tc_budget_non_claude, test_thinking_budget_non_claude);
    tcase_add_test(tc_budget_non_claude, test_thinking_budget_null_model);
    suite_add_tcase(s, tc_budget_non_claude);

    TCase *tc_budget_opus_4_5 = tcase_create("Thinking Budget - Opus 4.5");
    tcase_set_timeout(tc_budget_opus_4_5, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_budget_opus_4_5, setup, teardown);
    tcase_add_test(tc_budget_opus_4_5, test_thinking_budget_opus_4_5_none);
    tcase_add_test(tc_budget_opus_4_5, test_thinking_budget_opus_4_5_low);
    tcase_add_test(tc_budget_opus_4_5, test_thinking_budget_opus_4_5_med);
    tcase_add_test(tc_budget_opus_4_5, test_thinking_budget_opus_4_5_high);
    suite_add_tcase(s, tc_budget_opus_4_5);

    TCase *tc_adaptive = tcase_create("Adaptive Thinking - Model Detection");
    tcase_set_timeout(tc_adaptive, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_adaptive, setup, teardown);
    tcase_add_test(tc_adaptive, test_is_adaptive_model_opus_4_6);
    tcase_add_test(tc_adaptive, test_is_adaptive_model_sonnet_4_6);
    tcase_add_test(tc_adaptive, test_is_adaptive_model_sonnet_4_5);
    tcase_add_test(tc_adaptive, test_is_adaptive_model_haiku_4_5);
    tcase_add_test(tc_adaptive, test_is_adaptive_model_opus_4_5);
    tcase_add_test(tc_adaptive, test_is_adaptive_model_null);
    suite_add_tcase(s, tc_adaptive);

    TCase *tc_effort = tcase_create("Adaptive Thinking - Effort Mapping");
    tcase_set_timeout(tc_effort, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_effort, setup, teardown);
    tcase_add_test(tc_effort, test_thinking_effort_opus_4_6_none);
    tcase_add_test(tc_effort, test_thinking_effort_opus_4_6_low);
    tcase_add_test(tc_effort, test_thinking_effort_opus_4_6_med);
    tcase_add_test(tc_effort, test_thinking_effort_opus_4_6_high);
    tcase_add_test(tc_effort, test_thinking_effort_sonnet_4_6_none);
    tcase_add_test(tc_effort, test_thinking_effort_sonnet_4_6_low);
    tcase_add_test(tc_effort, test_thinking_effort_sonnet_4_6_med);
    tcase_add_test(tc_effort, test_thinking_effort_sonnet_4_6_high);
    tcase_add_test(tc_effort, test_thinking_effort_non_adaptive_model);
    tcase_add_test(tc_effort, test_thinking_effort_null_model);
    suite_add_tcase(s, tc_effort);

    return s;
}

int main(void)
{
    Suite *s = anthropic_thinking_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/anthropic/anthropic_thinking_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
