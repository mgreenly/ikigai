#include "../../../test_constants.h"
/**
 * @file reasoning_test.c
 * @brief Unit tests for OpenAI reasoning effort mapping
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/openai/reasoning.h"
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
 * ik_openai_is_reasoning_model Tests
 * ================================================================ */

START_TEST(test_is_reasoning_model_null) {
    bool result = ik_openai_is_reasoning_model(NULL);
    ck_assert(!result);
}
END_TEST

START_TEST(test_is_reasoning_model_empty) {
    bool result = ik_openai_is_reasoning_model("");
    ck_assert(!result);
}

END_TEST

START_TEST(test_is_reasoning_model_o1) {
    bool result = ik_openai_is_reasoning_model("o1");
    ck_assert(result);
}

END_TEST

START_TEST(test_is_reasoning_model_o1_mini) {
    bool result = ik_openai_is_reasoning_model("o1-mini");
    ck_assert(result);
}

END_TEST

START_TEST(test_is_reasoning_model_o1_preview) {
    bool result = ik_openai_is_reasoning_model("o1-preview");
    ck_assert(result);
}

END_TEST

START_TEST(test_is_reasoning_model_o3) {
    bool result = ik_openai_is_reasoning_model("o3");
    ck_assert(result);
}

END_TEST

START_TEST(test_is_reasoning_model_o3_mini) {
    bool result = ik_openai_is_reasoning_model("o3-mini");
    ck_assert(result);
}

END_TEST

START_TEST(test_is_reasoning_model_gpt5) {
    bool result = ik_openai_is_reasoning_model("gpt-5");
    ck_assert(result);
}

END_TEST

START_TEST(test_is_reasoning_model_gpt5_mini) {
    bool result = ik_openai_is_reasoning_model("gpt-5-mini");
    ck_assert(result);
}

END_TEST

START_TEST(test_is_reasoning_model_gpt5_pro) {
    bool result = ik_openai_is_reasoning_model("gpt-5-pro");
    ck_assert(result);
}

END_TEST

START_TEST(test_is_reasoning_model_gpt5_1) {
    bool result = ik_openai_is_reasoning_model("gpt-5.1");
    ck_assert(result);
}

END_TEST

START_TEST(test_is_reasoning_model_gpt5_2_codex) {
    bool result = ik_openai_is_reasoning_model("gpt-5.2-codex");
    ck_assert(result);
}

END_TEST

START_TEST(test_is_reasoning_model_gpt4) {
    bool result = ik_openai_is_reasoning_model("gpt-4");
    ck_assert(!result);
}

END_TEST

START_TEST(test_is_reasoning_model_gpt4o) {
    bool result = ik_openai_is_reasoning_model("gpt-4o");
    ck_assert(!result);
}

END_TEST

START_TEST(test_is_reasoning_model_claude) {
    bool result = ik_openai_is_reasoning_model("claude-3-5-sonnet");
    ck_assert(!result);
}

END_TEST
/* ================================================================
 * ik_openai_reasoning_effort Tests
 * ================================================================ */

// NULL model tests
START_TEST(test_reasoning_effort_null_model) {
    const char *effort = ik_openai_reasoning_effort(NULL, IK_THINKING_LOW);
    ck_assert_ptr_null(effort);
}

END_TEST

// o1/o3 family tests (NONE->low, LOW->low, MED->medium, HIGH->high)
START_TEST(test_reasoning_effort_o1_none) {
    const char *effort = ik_openai_reasoning_effort("o1", IK_THINKING_NONE);
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(effort, "low");
}

END_TEST

START_TEST(test_reasoning_effort_o1_low) {
    const char *effort = ik_openai_reasoning_effort("o1", IK_THINKING_LOW);
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(effort, "low");
}

END_TEST

START_TEST(test_reasoning_effort_o1_med) {
    const char *effort = ik_openai_reasoning_effort("o1", IK_THINKING_MED);
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(effort, "medium");
}

END_TEST

START_TEST(test_reasoning_effort_o1_high) {
    const char *effort = ik_openai_reasoning_effort("o1", IK_THINKING_HIGH);
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(effort, "high");
}

END_TEST

START_TEST(test_reasoning_effort_o3_mini_none) {
    const char *effort = ik_openai_reasoning_effort("o3-mini", IK_THINKING_NONE);
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(effort, "low");
}

END_TEST

// gpt-5 family tests (NONE->NULL, LOW->low, MED->medium, HIGH->high)
START_TEST(test_reasoning_effort_gpt5_none) {
    const char *effort = ik_openai_reasoning_effort("gpt-5", IK_THINKING_NONE);
    ck_assert_ptr_null(effort);
}

END_TEST

START_TEST(test_reasoning_effort_gpt5_low) {
    const char *effort = ik_openai_reasoning_effort("gpt-5", IK_THINKING_LOW);
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(effort, "low");
}

END_TEST

START_TEST(test_reasoning_effort_gpt5_med) {
    const char *effort = ik_openai_reasoning_effort("gpt-5", IK_THINKING_MED);
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(effort, "medium");
}

END_TEST

START_TEST(test_reasoning_effort_gpt5_high) {
    const char *effort = ik_openai_reasoning_effort("gpt-5", IK_THINKING_HIGH);
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(effort, "high");
}

END_TEST


// gpt-5-pro tests (all levels -> "high")
START_TEST(test_reasoning_effort_gpt5_pro_none) {
    const char *effort = ik_openai_reasoning_effort("gpt-5-pro", IK_THINKING_NONE);
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(effort, "high");
}

END_TEST

START_TEST(test_reasoning_effort_gpt5_pro_low) {
    const char *effort = ik_openai_reasoning_effort("gpt-5-pro", IK_THINKING_LOW);
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(effort, "high");
}

END_TEST

START_TEST(test_reasoning_effort_gpt5_pro_med) {
    const char *effort = ik_openai_reasoning_effort("gpt-5-pro", IK_THINKING_MED);
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(effort, "high");
}

END_TEST

START_TEST(test_reasoning_effort_gpt5_pro_high) {
    const char *effort = ik_openai_reasoning_effort("gpt-5-pro", IK_THINKING_HIGH);
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(effort, "high");
}

END_TEST

// Non-reasoning model test
START_TEST(test_reasoning_effort_gpt4) {
    const char *effort = ik_openai_reasoning_effort("gpt-4", IK_THINKING_LOW);
    ck_assert_ptr_null(effort);
}

END_TEST

// Invalid thinking level test
START_TEST(test_reasoning_effort_invalid_level) {
    const char *effort = ik_openai_reasoning_effort("o1", (ik_thinking_level_t)999);
    ck_assert_ptr_null(effort);
}

END_TEST
/* ================================================================
 * ik_openai_supports_temperature Tests
 * ================================================================ */

START_TEST(test_supports_temperature_gpt4) {
    bool result = ik_openai_supports_temperature("gpt-4");
    ck_assert(result);
}

END_TEST

START_TEST(test_supports_temperature_gpt4o) {
    bool result = ik_openai_supports_temperature("gpt-4o");
    ck_assert(result);
}

END_TEST

START_TEST(test_supports_temperature_o1) {
    // Reasoning models do NOT support temperature
    bool result = ik_openai_supports_temperature("o1");
    ck_assert(!result);
}

END_TEST

START_TEST(test_supports_temperature_o1_mini) {
    bool result = ik_openai_supports_temperature("o1-mini");
    ck_assert(!result);
}

END_TEST

START_TEST(test_supports_temperature_o3) {
    bool result = ik_openai_supports_temperature("o3");
    ck_assert(!result);
}

END_TEST

START_TEST(test_supports_temperature_gpt5) {
    // GPT-5 reasoning models do NOT support temperature
    bool result = ik_openai_supports_temperature("gpt-5");
    ck_assert(!result);
}

END_TEST

/* ================================================================
 * ik_openai_prefer_responses_api Tests
 * ================================================================ */

START_TEST(test_prefer_responses_api_gpt4) {
    bool result = ik_openai_prefer_responses_api("gpt-4");
    ck_assert(!result);
}

END_TEST

START_TEST(test_prefer_responses_api_o1) {
    // Reasoning models prefer Responses API
    bool result = ik_openai_prefer_responses_api("o1");
    ck_assert(result);
}

END_TEST

START_TEST(test_prefer_responses_api_o3_mini) {
    bool result = ik_openai_prefer_responses_api("o3-mini");
    ck_assert(result);
}

END_TEST

START_TEST(test_prefer_responses_api_gpt5) {
    // GPT-5 reasoning models prefer Responses API
    bool result = ik_openai_prefer_responses_api("gpt-5");
    ck_assert(result);
}

END_TEST

/* ================================================================
 * ik_openai_validate_thinking Tests
 * ================================================================ */

START_TEST(test_validate_thinking_null_model) {
    res_t r = ik_openai_validate_thinking(test_ctx, NULL, IK_THINKING_LOW);
    ck_assert(is_err(&r));
    ck_assert_ptr_nonnull(r.err);
    ck_assert_int_eq(r.err->code, ERR_INVALID_ARG);
    ck_assert(strstr(r.err->msg, "Model cannot be NULL") != NULL);
}

END_TEST

START_TEST(test_validate_thinking_none_always_valid) {
    // NONE is valid for any model
    res_t r = ik_openai_validate_thinking(test_ctx, "gpt-4", IK_THINKING_NONE);
    ck_assert(!is_err(&r));
}

END_TEST

START_TEST(test_validate_thinking_none_reasoning_model) {
    // NONE is valid for reasoning models too
    res_t r = ik_openai_validate_thinking(test_ctx, "o1", IK_THINKING_NONE);
    ck_assert(!is_err(&r));
}

END_TEST

START_TEST(test_validate_thinking_low_non_reasoning) {
    // LOW is invalid for non-reasoning models
    res_t r = ik_openai_validate_thinking(test_ctx, "gpt-4", IK_THINKING_LOW);
    ck_assert(is_err(&r));
    ck_assert_ptr_nonnull(r.err);
    ck_assert_int_eq(r.err->code, ERR_INVALID_ARG);
    ck_assert(strstr(r.err->msg, "does not support thinking") != NULL);
}

END_TEST

START_TEST(test_validate_thinking_med_non_reasoning) {
    // MED is invalid for non-reasoning models
    res_t r = ik_openai_validate_thinking(test_ctx, "gpt-4o", IK_THINKING_MED);
    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_INVALID_ARG);
}

END_TEST

START_TEST(test_validate_thinking_high_non_reasoning) {
    // HIGH is invalid for non-reasoning models
    res_t r = ik_openai_validate_thinking(test_ctx, "gpt-4", IK_THINKING_HIGH);
    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_INVALID_ARG);
}

END_TEST

START_TEST(test_validate_thinking_low_reasoning) {
    // LOW is valid for reasoning models
    res_t r = ik_openai_validate_thinking(test_ctx, "o1", IK_THINKING_LOW);
    ck_assert(!is_err(&r));
}

END_TEST

START_TEST(test_validate_thinking_med_reasoning) {
    // MED is valid for reasoning models
    res_t r = ik_openai_validate_thinking(test_ctx, "o3-mini", IK_THINKING_MED);
    ck_assert(!is_err(&r));
}

END_TEST

START_TEST(test_validate_thinking_high_reasoning) {
    // HIGH is valid for reasoning models
    res_t r = ik_openai_validate_thinking(test_ctx, "o1-preview", IK_THINKING_HIGH);
    ck_assert(!is_err(&r));
}

END_TEST

START_TEST(test_validate_thinking_gpt5_low) {
    // GPT-5 models support thinking
    res_t r = ik_openai_validate_thinking(test_ctx, "gpt-5", IK_THINKING_LOW);
    ck_assert(!is_err(&r));
}

END_TEST


/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *reasoning_suite(void)
{
    Suite *s = suite_create("OpenAI Reasoning");

    TCase *tc_is_reasoning = tcase_create("is_reasoning_model");
    tcase_set_timeout(tc_is_reasoning, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_is_reasoning, setup, teardown);
    tcase_add_test(tc_is_reasoning, test_is_reasoning_model_null);
    tcase_add_test(tc_is_reasoning, test_is_reasoning_model_empty);
    tcase_add_test(tc_is_reasoning, test_is_reasoning_model_o1);
    tcase_add_test(tc_is_reasoning, test_is_reasoning_model_o1_mini);
    tcase_add_test(tc_is_reasoning, test_is_reasoning_model_o1_preview);
    tcase_add_test(tc_is_reasoning, test_is_reasoning_model_o3);
    tcase_add_test(tc_is_reasoning, test_is_reasoning_model_o3_mini);
    tcase_add_test(tc_is_reasoning, test_is_reasoning_model_gpt5);
    tcase_add_test(tc_is_reasoning, test_is_reasoning_model_gpt5_mini);
    tcase_add_test(tc_is_reasoning, test_is_reasoning_model_gpt5_pro);
    tcase_add_test(tc_is_reasoning, test_is_reasoning_model_gpt5_1);
    tcase_add_test(tc_is_reasoning, test_is_reasoning_model_gpt5_2_codex);
    tcase_add_test(tc_is_reasoning, test_is_reasoning_model_gpt4);
    tcase_add_test(tc_is_reasoning, test_is_reasoning_model_gpt4o);
    tcase_add_test(tc_is_reasoning, test_is_reasoning_model_claude);
    suite_add_tcase(s, tc_is_reasoning);

    TCase *tc_effort = tcase_create("reasoning_effort");
    tcase_set_timeout(tc_effort, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_effort, setup, teardown);
    tcase_add_test(tc_effort, test_reasoning_effort_null_model);
    tcase_add_test(tc_effort, test_reasoning_effort_o1_none);
    tcase_add_test(tc_effort, test_reasoning_effort_o1_low);
    tcase_add_test(tc_effort, test_reasoning_effort_o1_med);
    tcase_add_test(tc_effort, test_reasoning_effort_o1_high);
    tcase_add_test(tc_effort, test_reasoning_effort_o3_mini_none);
    tcase_add_test(tc_effort, test_reasoning_effort_gpt5_none);
    tcase_add_test(tc_effort, test_reasoning_effort_gpt5_low);
    tcase_add_test(tc_effort, test_reasoning_effort_gpt5_med);
    tcase_add_test(tc_effort, test_reasoning_effort_gpt5_high);
    tcase_add_test(tc_effort, test_reasoning_effort_gpt5_pro_none);
    tcase_add_test(tc_effort, test_reasoning_effort_gpt5_pro_low);
    tcase_add_test(tc_effort, test_reasoning_effort_gpt5_pro_med);
    tcase_add_test(tc_effort, test_reasoning_effort_gpt5_pro_high);
    tcase_add_test(tc_effort, test_reasoning_effort_gpt4);
    tcase_add_test(tc_effort, test_reasoning_effort_invalid_level);
    suite_add_tcase(s, tc_effort);

    TCase *tc_temperature = tcase_create("supports_temperature");
    tcase_set_timeout(tc_temperature, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_temperature, setup, teardown);
    tcase_add_test(tc_temperature, test_supports_temperature_gpt4);
    tcase_add_test(tc_temperature, test_supports_temperature_gpt4o);
    tcase_add_test(tc_temperature, test_supports_temperature_o1);
    tcase_add_test(tc_temperature, test_supports_temperature_o1_mini);
    tcase_add_test(tc_temperature, test_supports_temperature_o3);
    tcase_add_test(tc_temperature, test_supports_temperature_gpt5);
    suite_add_tcase(s, tc_temperature);

    TCase *tc_responses = tcase_create("prefer_responses_api");
    tcase_set_timeout(tc_responses, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_responses, setup, teardown);
    tcase_add_test(tc_responses, test_prefer_responses_api_gpt4);
    tcase_add_test(tc_responses, test_prefer_responses_api_o1);
    tcase_add_test(tc_responses, test_prefer_responses_api_o3_mini);
    tcase_add_test(tc_responses, test_prefer_responses_api_gpt5);
    suite_add_tcase(s, tc_responses);

    TCase *tc_validate = tcase_create("validate_thinking");
    tcase_set_timeout(tc_validate, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_validate, setup, teardown);
    tcase_add_test(tc_validate, test_validate_thinking_null_model);
    tcase_add_test(tc_validate, test_validate_thinking_none_always_valid);
    tcase_add_test(tc_validate, test_validate_thinking_none_reasoning_model);
    tcase_add_test(tc_validate, test_validate_thinking_low_non_reasoning);
    tcase_add_test(tc_validate, test_validate_thinking_med_non_reasoning);
    tcase_add_test(tc_validate, test_validate_thinking_high_non_reasoning);
    tcase_add_test(tc_validate, test_validate_thinking_low_reasoning);
    tcase_add_test(tc_validate, test_validate_thinking_med_reasoning);
    tcase_add_test(tc_validate, test_validate_thinking_high_reasoning);
    tcase_add_test(tc_validate, test_validate_thinking_gpt5_low);
    suite_add_tcase(s, tc_validate);

    return s;
}

int main(void)
{
    Suite *s = reasoning_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/providers/openai/reasoning_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
