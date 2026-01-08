/**
 * @file anthropic_response_test.c
 * @brief Unit tests for Anthropic response parsing
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/anthropic/response.h"
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
 * Finish Reason Mapping Tests
 * ================================================================ */

START_TEST(test_map_finish_reason_null) {
    ik_finish_reason_t reason = ik_anthropic_map_finish_reason(NULL);
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}

END_TEST

START_TEST(test_map_finish_reason_end_turn) {
    ik_finish_reason_t reason = ik_anthropic_map_finish_reason("end_turn");
    ck_assert_int_eq(reason, IK_FINISH_STOP);
}

END_TEST

START_TEST(test_map_finish_reason_max_tokens) {
    ik_finish_reason_t reason = ik_anthropic_map_finish_reason("max_tokens");
    ck_assert_int_eq(reason, IK_FINISH_LENGTH);
}

END_TEST

START_TEST(test_map_finish_reason_tool_use) {
    ik_finish_reason_t reason = ik_anthropic_map_finish_reason("tool_use");
    ck_assert_int_eq(reason, IK_FINISH_TOOL_USE);
}

END_TEST

START_TEST(test_map_finish_reason_stop_sequence) {
    ik_finish_reason_t reason = ik_anthropic_map_finish_reason("stop_sequence");
    ck_assert_int_eq(reason, IK_FINISH_STOP);
}

END_TEST

START_TEST(test_map_finish_reason_refusal) {
    ik_finish_reason_t reason = ik_anthropic_map_finish_reason("refusal");
    ck_assert_int_eq(reason, IK_FINISH_CONTENT_FILTER);
}

END_TEST

START_TEST(test_map_finish_reason_unknown) {
    ik_finish_reason_t reason = ik_anthropic_map_finish_reason("unknown_reason");
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}

END_TEST
/* ================================================================
 * Response Parsing Tests
 * ================================================================ */

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *anthropic_response_suite(void)
{
    Suite *s = suite_create("Anthropic Response");

    TCase *tc_finish = tcase_create("Finish Reason Mapping");
    tcase_set_timeout(tc_finish, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_finish, setup, teardown);
    tcase_add_test(tc_finish, test_map_finish_reason_null);
    tcase_add_test(tc_finish, test_map_finish_reason_end_turn);
    tcase_add_test(tc_finish, test_map_finish_reason_max_tokens);
    tcase_add_test(tc_finish, test_map_finish_reason_tool_use);
    tcase_add_test(tc_finish, test_map_finish_reason_stop_sequence);
    tcase_add_test(tc_finish, test_map_finish_reason_refusal);
    tcase_add_test(tc_finish, test_map_finish_reason_unknown);
    suite_add_tcase(s, tc_finish);

    return s;
}

int main(void)
{
    Suite *s = anthropic_response_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
