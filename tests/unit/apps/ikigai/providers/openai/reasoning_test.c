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
    const char *models[] = {"o1", "o3", "o3-mini", "o4-mini", "o3-pro",
        "gpt-5", "gpt-5-mini", "gpt-5-pro", "gpt-5.2", "gpt-5.2-codex",
        "gpt-5.1-codex-mini", "gpt-5.2-pro"};
    for (size_t i = 0; i < 12; i++) {
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
    ck_assert(!ik_openai_is_reasoning_model("gpt-4.1"));
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
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(effort, "minimal");
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
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(effort, "none");
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

// o3: can disable reasoning (unlike o1/o3-mini)
START_TEST(test_reasoning_effort_o3_none) {
    const char *effort = ik_openai_reasoning_effort("o3", IK_THINKING_NONE);
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(effort, "none");
} END_TEST

// o4-mini: new o-series, same as o3
START_TEST(test_reasoning_effort_o4_mini_none) {
    const char *effort = ik_openai_reasoning_effort("o4-mini", IK_THINKING_NONE);
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(effort, "none");
} END_TEST

START_TEST(test_reasoning_effort_o4_mini_high) {
    const char *effort = ik_openai_reasoning_effort("o4-mini", IK_THINKING_HIGH);
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(effort, "high");
} END_TEST

// o3-pro: same effort range as o3
START_TEST(test_reasoning_effort_o3_pro_none) {
    const char *effort = ik_openai_reasoning_effort("o3-pro", IK_THINKING_NONE);
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(effort, "none");
} END_TEST

// gpt-5.1-codex-mini: none family, max "high"
START_TEST(test_reasoning_effort_gpt51_codex_mini_none) {
    const char *effort = ik_openai_reasoning_effort("gpt-5.1-codex-mini", IK_THINKING_NONE);
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(effort, "none");
} END_TEST

START_TEST(test_reasoning_effort_gpt51_codex_mini_high) {
    const char *effort = ik_openai_reasoning_effort("gpt-5.1-codex-mini", IK_THINKING_HIGH);
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(effort, "high");
} END_TEST

// gpt-5.2: HIGH -> "xhigh"
START_TEST(test_reasoning_effort_gpt52_high) {
    const char *effort = ik_openai_reasoning_effort("gpt-5.2", IK_THINKING_HIGH);
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(effort, "xhigh");
} END_TEST

// gpt-5.2-pro: same xhigh ceiling as gpt-5.2
START_TEST(test_reasoning_effort_gpt52_pro_none) {
    const char *effort = ik_openai_reasoning_effort("gpt-5.2-pro", IK_THINKING_NONE);
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(effort, "none");
} END_TEST

START_TEST(test_reasoning_effort_gpt52_pro_high) {
    const char *effort = ik_openai_reasoning_effort("gpt-5.2-pro", IK_THINKING_HIGH);
    ck_assert_ptr_nonnull(effort);
    ck_assert_str_eq(effort, "xhigh");
} END_TEST

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
    const char *chat_models[] = {"gpt-4", "gpt-4-turbo", "gpt-4o", "gpt-4o-mini",
        "gpt-4.1", "gpt-4.1-mini", "gpt-4.1-nano"};
    for (size_t i = 0; i < 7; i++) {
        ck_assert(!ik_openai_use_responses_api(chat_models[i]));
    }
}

END_TEST

START_TEST(test_use_responses_api_responses) {
    const char *resp_models[] = {
        "o1", "o3", "o3-mini", "o4-mini", "o3-pro",
        "gpt-5", "gpt-5-mini", "gpt-5-nano", "gpt-5-pro",
        "gpt-5.1", "gpt-5.1-chat-latest", "gpt-5.1-codex", "gpt-5.1-codex-mini",
        "gpt-5.2", "gpt-5.2-chat-latest", "gpt-5.2-codex", "gpt-5.2-pro"
    };
    for (size_t i = 0; i < 17; i++) {
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
    tcase_add_test(tc_effort, test_reasoning_effort_o3_none);
    tcase_add_test(tc_effort, test_reasoning_effort_o4_mini_none);
    tcase_add_test(tc_effort, test_reasoning_effort_o4_mini_high);
    tcase_add_test(tc_effort, test_reasoning_effort_o3_pro_none);
    tcase_add_test(tc_effort, test_reasoning_effort_gpt51_codex_mini_none);
    tcase_add_test(tc_effort, test_reasoning_effort_gpt51_codex_mini_high);
    tcase_add_test(tc_effort, test_reasoning_effort_gpt52_high);
    tcase_add_test(tc_effort, test_reasoning_effort_gpt52_pro_none);
    tcase_add_test(tc_effort, test_reasoning_effort_gpt52_pro_high);
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
