#include "tests/test_constants.h"
/**
 * @file response_responses_main_test.c
 * @brief Tests for OpenAI Responses API main parsing and status coverage
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
 * Coverage Tests for main parsing function
 * ================================================================ */

START_TEST(test_parse_response_root_not_object) {
    const char *json = "[]";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
}

END_TEST

START_TEST(test_parse_response_error_with_message) {
    const char *json = "{"
                       "\"error\":{"
                       "\"message\":\"Test error message\","
                       "\"code\":\"test_error\""
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
}

END_TEST

START_TEST(test_parse_response_error_message_not_string) {
    const char *json = "{"
                       "\"error\":{"
                       "\"message\":123,"
                       "\"code\":\"test_error\""
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
}

END_TEST

START_TEST(test_parse_response_incomplete_details_reason_null) {
    const char *json = "{"
                       "\"id\":\"resp-incomplete\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"incomplete\","
                       "\"incomplete_details\":{"
                       "\"reason\":null"
                       "},"
                       "\"output\":[]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_LENGTH);
}

END_TEST

START_TEST(test_parse_response_model_null) {
    const char *json = "{"
                       "\"id\":\"resp-no-model\","
                       "\"model\":null,"
                       "\"status\":\"completed\","
                       "\"output\":[]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
}

END_TEST

START_TEST(test_parse_response_status_null) {
    const char *json = "{"
                       "\"id\":\"resp-no-status\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":null,"
                       "\"output\":[]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_UNKNOWN);
}

END_TEST

START_TEST(test_parse_response_output_not_array) {
    const char *json = "{"
                       "\"id\":\"resp-bad-output\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":\"not an array\""
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
}

END_TEST

START_TEST(test_parse_response_output_null) {
    const char *json = "{"
                       "\"id\":\"resp-null-output\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":null"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
}

END_TEST

START_TEST(test_parse_response_invalid_json) {
    const char *json = "{not valid json}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
}

END_TEST

START_TEST(test_parse_response_error_without_message) {
    const char *json = "{"
                       "\"error\":{"
                       "\"code\":\"test_error\""
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
}

END_TEST

START_TEST(test_parse_response_error_message_null) {
    const char *json = "{"
                       "\"error\":{"
                       "\"message\":null,"
                       "\"code\":\"test_error\""
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
}

END_TEST

START_TEST(test_parse_response_model_not_string) {
    const char *json = "{"
                       "\"id\":\"resp-model\","
                       "\"model\":123,"
                       "\"status\":\"completed\","
                       "\"output\":[]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
}

END_TEST

START_TEST(test_parse_response_status_not_string) {
    const char *json = "{"
                       "\"id\":\"resp-status\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":123,"
                       "\"output\":[]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_UNKNOWN);
}

END_TEST

START_TEST(test_parse_response_incomplete_details_not_object) {
    const char *json = "{"
                       "\"id\":\"resp-incomplete\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"incomplete\","
                       "\"incomplete_details\":\"not an object\","
                       "\"output\":[]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_LENGTH);
}

END_TEST

START_TEST(test_parse_response_incomplete_reason_not_string) {
    const char *json = "{"
                       "\"id\":\"resp-incomplete\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"incomplete\","
                       "\"incomplete_details\":{"
                       "\"reason\":123"
                       "},"
                       "\"output\":[]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_LENGTH);
}

END_TEST

START_TEST(test_parse_response_no_output_field) {
    const char *json = "{"
                       "\"id\":\"resp-no-output\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\""
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
}

END_TEST

START_TEST(test_parse_response_no_incomplete_details) {
    const char *json = "{"
                       "\"id\":\"resp-incomplete\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"incomplete\","
                       "\"output\":[]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_LENGTH);
}

END_TEST

/* ================================================================
 * Coverage Tests for status mapping
 * ================================================================ */

START_TEST(test_map_status_null) {
    ik_finish_reason_t reason = ik_openai_map_responses_status(NULL, NULL);
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}

END_TEST

START_TEST(test_map_status_failed) {
    ik_finish_reason_t reason = ik_openai_map_responses_status("failed", NULL);
    ck_assert_int_eq(reason, IK_FINISH_ERROR);
}

END_TEST

START_TEST(test_map_status_cancelled) {
    ik_finish_reason_t reason = ik_openai_map_responses_status("cancelled", NULL);
    ck_assert_int_eq(reason, IK_FINISH_STOP);
}

END_TEST

START_TEST(test_map_status_incomplete_content_filter) {
    ik_finish_reason_t reason = ik_openai_map_responses_status("incomplete", "content_filter");
    ck_assert_int_eq(reason, IK_FINISH_CONTENT_FILTER);
}

END_TEST

START_TEST(test_map_status_incomplete_max_tokens) {
    ik_finish_reason_t reason = ik_openai_map_responses_status("incomplete", "max_output_tokens");
    ck_assert_int_eq(reason, IK_FINISH_LENGTH);
}

END_TEST

START_TEST(test_map_status_incomplete_unknown_reason) {
    ik_finish_reason_t reason = ik_openai_map_responses_status("incomplete", "unknown_reason");
    ck_assert_int_eq(reason, IK_FINISH_LENGTH);
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *response_responses_main_suite(void)
{
    Suite *s = suite_create("OpenAI Responses API Main Parsing Tests");

    TCase *tc_main = tcase_create("Main Parsing Coverage");
    tcase_set_timeout(tc_main, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_main, setup, teardown);
    tcase_add_test(tc_main, test_parse_response_root_not_object);
    tcase_add_test(tc_main, test_parse_response_error_with_message);
    tcase_add_test(tc_main, test_parse_response_error_message_not_string);
    tcase_add_test(tc_main, test_parse_response_incomplete_details_reason_null);
    tcase_add_test(tc_main, test_parse_response_model_null);
    tcase_add_test(tc_main, test_parse_response_status_null);
    tcase_add_test(tc_main, test_parse_response_output_not_array);
    tcase_add_test(tc_main, test_parse_response_output_null);
    tcase_add_test(tc_main, test_parse_response_invalid_json);
    tcase_add_test(tc_main, test_parse_response_error_without_message);
    tcase_add_test(tc_main, test_parse_response_error_message_null);
    tcase_add_test(tc_main, test_parse_response_model_not_string);
    tcase_add_test(tc_main, test_parse_response_status_not_string);
    tcase_add_test(tc_main, test_parse_response_incomplete_details_not_object);
    tcase_add_test(tc_main, test_parse_response_incomplete_reason_not_string);
    tcase_add_test(tc_main, test_parse_response_no_output_field);
    tcase_add_test(tc_main, test_parse_response_no_incomplete_details);
    suite_add_tcase(s, tc_main);

    TCase *tc_status = tcase_create("Status Mapping Coverage");
    tcase_set_timeout(tc_status, IK_TEST_TIMEOUT);
    tcase_add_test(tc_status, test_map_status_null);
    tcase_add_test(tc_status, test_map_status_failed);
    tcase_add_test(tc_status, test_map_status_cancelled);
    tcase_add_test(tc_status, test_map_status_incomplete_content_filter);
    tcase_add_test(tc_status, test_map_status_incomplete_max_tokens);
    tcase_add_test(tc_status, test_map_status_incomplete_unknown_reason);
    suite_add_tcase(s, tc_status);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = response_responses_main_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/openai/response_responses_main_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
