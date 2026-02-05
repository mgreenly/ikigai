#include "tests/test_constants.h"
/**
 * @file response_responses_status_test.c
 * @brief Tests for OpenAI Responses API status mapping
 */

#include "apps/ikigai/providers/openai/response.h"
#include "apps/ikigai/providers/provider.h"
#include "shared/error.h"

#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

/* Test fixture */
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
 * Status Mapping Tests
 * ================================================================ */

START_TEST(test_map_responses_status_null) {
    ik_finish_reason_t reason = ik_openai_map_responses_status(NULL, NULL);
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}

END_TEST

START_TEST(test_map_responses_status_completed) {
    ik_finish_reason_t reason = ik_openai_map_responses_status("completed", NULL);
    ck_assert_int_eq(reason, IK_FINISH_STOP);
}

END_TEST

START_TEST(test_map_responses_status_failed) {
    ik_finish_reason_t reason = ik_openai_map_responses_status("failed", NULL);
    ck_assert_int_eq(reason, IK_FINISH_ERROR);
}

END_TEST

START_TEST(test_map_responses_status_cancelled) {
    ik_finish_reason_t reason = ik_openai_map_responses_status("cancelled", NULL);
    ck_assert_int_eq(reason, IK_FINISH_STOP);
}

END_TEST

START_TEST(test_map_responses_status_incomplete_max_tokens) {
    ik_finish_reason_t reason = ik_openai_map_responses_status("incomplete", "max_output_tokens");
    ck_assert_int_eq(reason, IK_FINISH_LENGTH);
}

END_TEST

START_TEST(test_map_responses_status_incomplete_content_filter) {
    ik_finish_reason_t reason = ik_openai_map_responses_status("incomplete", "content_filter");
    ck_assert_int_eq(reason, IK_FINISH_CONTENT_FILTER);
}

END_TEST

START_TEST(test_map_responses_status_incomplete_null_reason) {
    ik_finish_reason_t reason = ik_openai_map_responses_status("incomplete", NULL);
    ck_assert_int_eq(reason, IK_FINISH_LENGTH);
}

END_TEST

START_TEST(test_map_responses_status_incomplete_unknown_reason) {
    ik_finish_reason_t reason = ik_openai_map_responses_status("incomplete", "other_reason");
    ck_assert_int_eq(reason, IK_FINISH_LENGTH);
}

END_TEST

START_TEST(test_map_responses_status_unknown) {
    ik_finish_reason_t reason = ik_openai_map_responses_status("unknown_status", NULL);
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *response_responses_status_suite(void)
{
    Suite *s = suite_create("OpenAI Responses API Status Mapping");

    TCase *tc_status = tcase_create("Status Mapping");
    tcase_set_timeout(tc_status, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_status, setup, teardown);
    tcase_add_test(tc_status, test_map_responses_status_null);
    tcase_add_test(tc_status, test_map_responses_status_completed);
    tcase_add_test(tc_status, test_map_responses_status_failed);
    tcase_add_test(tc_status, test_map_responses_status_cancelled);
    tcase_add_test(tc_status, test_map_responses_status_incomplete_max_tokens);
    tcase_add_test(tc_status, test_map_responses_status_incomplete_content_filter);
    tcase_add_test(tc_status, test_map_responses_status_incomplete_null_reason);
    tcase_add_test(tc_status, test_map_responses_status_incomplete_unknown_reason);
    tcase_add_test(tc_status, test_map_responses_status_unknown);
    suite_add_tcase(s, tc_status);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = response_responses_status_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/openai/response_responses_status_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
