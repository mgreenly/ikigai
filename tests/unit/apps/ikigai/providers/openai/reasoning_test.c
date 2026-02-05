#include "tests/test_constants.h"
/**
 * @file reasoning_test.c
 * @brief Unit tests for OpenAI reasoning effort mapping
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/providers/openai/reasoning.h"
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

/* ================================================================
 * ik_openai_is_reasoning_model Tests
 * ================================================================ */

START_TEST(test_is_reasoning_model_reasoning) {
    const char *models[] = {"o1", "o1-mini", "o1-preview", "o3", "o3-mini",
        "gpt-5", "gpt-5-mini", "gpt-5-pro", "gpt-5.2", "gpt-5.2-codex"};
    for (size_t i = 0; i < 10; i++) {
        ck_assert(ik_openai_is_reasoning_model(models[i]));
    }
}

END_TEST

START_TEST(test_is_reasoning_model_non_reasoning) {
    ck_assert(!ik_openai_is_reasoning_model(NULL));
    ck_assert(!ik_openai_is_reasoning_model(""));
    ck_assert(!ik_openai_is_reasoning_model("gpt-4"));
    ck_assert(!ik_openai_is_reasoning_model("gpt-4o"));
    ck_assert(!ik_openai_is_reasoning_model("claude-3-5-sonnet"));
}

END_TEST
/* ================================================================
 * ik_openai_reasoning_effort Tests
 * ================================================================ */

// o1/o3 family tests
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

// gpt-5.x family tests
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

START_TEST(test_reasoning_effort_gpt52_none) {
    const char *effort = ik_openai_reasoning_effort("gpt-5.2", IK_THINKING_NONE);
    ck_assert_ptr_null(effort);
}

END_TEST

START_TEST(test_reasoning_effort_gpt52_codex_low) {
    const char *effort = ik_openai_reasoning_effort("gpt-5.2-codex", IK_THINKING_LOW);
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(effort, "low");
}

END_TEST

// gpt-5-pro tests
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

// Invalid/edge cases
START_TEST(test_reasoning_effort_null_model) {
    const char *effort = ik_openai_reasoning_effort(NULL, IK_THINKING_LOW);
    ck_assert_ptr_null(effort);
}

END_TEST

START_TEST(test_reasoning_effort_invalid_level) {
    const char *effort = ik_openai_reasoning_effort("o1", (ik_thinking_level_t)999);
    ck_assert_ptr_null(effort);
}

END_TEST

/* ================================================================
 * ik_openai_use_responses_api Tests
 * ================================================================ */

START_TEST(test_use_responses_api_chat_completions) {
    const char *chat_models[] = {"gpt-4", "gpt-4-turbo", "gpt-4o", "gpt-4o-mini"};
    for (size_t i = 0; i < 4; i++) {
        ck_assert(!ik_openai_use_responses_api(chat_models[i]));
    }
}

END_TEST

START_TEST(test_use_responses_api_responses) {
    const char *resp_models[] = {
        "o1", "o1-mini", "o1-preview", "o3", "o3-mini",
        "gpt-5", "gpt-5-mini", "gpt-5-nano", "gpt-5-pro",
        "gpt-5.1", "gpt-5.1-chat-latest", "gpt-5.1-codex",
        "gpt-5.2", "gpt-5.2-chat-latest", "gpt-5.2-codex"
    };
    for (size_t i = 0; i < 15; i++) {
        ck_assert(ik_openai_use_responses_api(resp_models[i]));
    }
}

END_TEST

START_TEST(test_use_responses_api_edge_cases) {
    ck_assert(!ik_openai_use_responses_api(NULL));
    ck_assert(!ik_openai_use_responses_api(""));
    ck_assert(!ik_openai_use_responses_api("gpt-7"));
    ck_assert(!ik_openai_use_responses_api("unknown-model"));
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
    res_t r = ik_openai_validate_thinking(test_ctx, "gpt-4o-mini", IK_THINKING_HIGH);
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

START_TEST(test_validate_thinking_gpt52_med) {
    // GPT-5.2 models support thinking
    res_t r = ik_openai_validate_thinking(test_ctx, "gpt-5.2", IK_THINKING_MED);
    ck_assert(!is_err(&r));
}

END_TEST

START_TEST(test_validate_thinking_gpt5_pro_high) {
    // GPT-5-pro supports thinking
    res_t r = ik_openai_validate_thinking(test_ctx, "gpt-5-pro", IK_THINKING_HIGH);
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
    tcase_add_test(tc_is_reasoning, test_is_reasoning_model_reasoning);
    tcase_add_test(tc_is_reasoning, test_is_reasoning_model_non_reasoning);
    suite_add_tcase(s, tc_is_reasoning);

    TCase *tc_effort = tcase_create("reasoning_effort");
    tcase_set_timeout(tc_effort, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_effort, setup, teardown);
    tcase_add_test(tc_effort, test_reasoning_effort_o1_none);
    tcase_add_test(tc_effort, test_reasoning_effort_o1_low);
    tcase_add_test(tc_effort, test_reasoning_effort_o1_med);
    tcase_add_test(tc_effort, test_reasoning_effort_o1_high);
    tcase_add_test(tc_effort, test_reasoning_effort_o3_mini_none);
    tcase_add_test(tc_effort, test_reasoning_effort_gpt5_none);
    tcase_add_test(tc_effort, test_reasoning_effort_gpt5_low);
    tcase_add_test(tc_effort, test_reasoning_effort_gpt5_med);
    tcase_add_test(tc_effort, test_reasoning_effort_gpt5_high);
    tcase_add_test(tc_effort, test_reasoning_effort_gpt52_none);
    tcase_add_test(tc_effort, test_reasoning_effort_gpt52_codex_low);
    tcase_add_test(tc_effort, test_reasoning_effort_gpt5_pro_none);
    tcase_add_test(tc_effort, test_reasoning_effort_gpt5_pro_low);
    tcase_add_test(tc_effort, test_reasoning_effort_gpt5_pro_med);
    tcase_add_test(tc_effort, test_reasoning_effort_gpt5_pro_high);
    tcase_add_test(tc_effort, test_reasoning_effort_null_model);
    tcase_add_test(tc_effort, test_reasoning_effort_invalid_level);
    suite_add_tcase(s, tc_effort);

    TCase *tc_responses = tcase_create("use_responses_api");
    tcase_set_timeout(tc_responses, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_responses, setup, teardown);
    tcase_add_test(tc_responses, test_use_responses_api_chat_completions);
    tcase_add_test(tc_responses, test_use_responses_api_responses);
    tcase_add_test(tc_responses, test_use_responses_api_edge_cases);
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
    tcase_add_test(tc_validate, test_validate_thinking_gpt52_med);
    tcase_add_test(tc_validate, test_validate_thinking_gpt5_pro_high);
    suite_add_tcase(s, tc_validate);

    return s;
}

int main(void)
{
    Suite *s = reasoning_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/openai/reasoning_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
