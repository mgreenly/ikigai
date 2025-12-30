/**
 * @file response_responses_coverage_test.c
 * @brief Tests for OpenAI Responses API coverage gaps
 */

#include "../../../../src/providers/openai/response.h"
#include "../../../../src/providers/provider.h"
#include "../../../../src/error.h"

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
 * Coverage Tests for parse_usage
 * ================================================================ */

START_TEST(test_parse_usage_prompt_tokens_not_int) {
    const char *json = "{"
                       "\"id\":\"resp-usage\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[],"
                       "\"usage\":{"
                       "\"prompt_tokens\":\"not an int\","
                       "\"completion_tokens\":10,"
                       "\"total_tokens\":15"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->usage.input_tokens, 0);
    ck_assert_int_eq(resp->usage.output_tokens, 10);
    ck_assert_int_eq(resp->usage.total_tokens, 15);
}

END_TEST

START_TEST(test_parse_usage_completion_tokens_not_int) {
    const char *json = "{"
                       "\"id\":\"resp-usage\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":\"not an int\","
                       "\"total_tokens\":15"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->usage.input_tokens, 5);
    ck_assert_int_eq(resp->usage.output_tokens, 0);
    ck_assert_int_eq(resp->usage.total_tokens, 15);
}

END_TEST

START_TEST(test_parse_usage_total_tokens_not_int) {
    const char *json = "{"
                       "\"id\":\"resp-usage\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":10,"
                       "\"total_tokens\":\"not an int\""
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->usage.input_tokens, 5);
    ck_assert_int_eq(resp->usage.output_tokens, 10);
    ck_assert_int_eq(resp->usage.total_tokens, 0);
}

END_TEST

START_TEST(test_parse_usage_reasoning_tokens_not_int) {
    const char *json = "{"
                       "\"id\":\"resp-usage\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":10,"
                       "\"total_tokens\":15,"
                       "\"completion_tokens_details\":{"
                       "\"reasoning_tokens\":\"not an int\""
                       "}"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->usage.thinking_tokens, 0);
}

END_TEST

/* ================================================================
 * Coverage Tests for parse_function_call
 * ================================================================ */

START_TEST(test_parse_function_call_id_null) {
    const char *json = "{"
                       "\"id\":\"resp-func\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"function_call\","
                       "\"id\":null,"
                       "\"name\":\"test_func\","
                       "\"arguments\":\"{}\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
}

END_TEST

/* ================================================================
 * Coverage Tests for count_content_blocks
 * ================================================================ */

START_TEST(test_count_content_blocks_type_null) {
    const char *json = "{"
                       "\"id\":\"resp-count\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":null"
                       "},{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":\"output_text\","
                       "\"text\":\"Hello\""
                       "}]"
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 1);
}

END_TEST

START_TEST(test_count_content_blocks_type_not_string) {
    const char *json = "{"
                       "\"id\":\"resp-count\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":123"
                       "},{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":\"output_text\","
                       "\"text\":\"Hello\""
                       "}]"
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 1);
}

END_TEST

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

START_TEST(test_parse_function_call_with_call_id) {
    const char *json = "{"
                       "\"id\":\"resp-func\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"function_call\","
                       "\"id\":\"old-id\","
                       "\"call_id\":\"new-id\","
                       "\"name\":\"test_func\","
                       "\"arguments\":\"{}\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 1);
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.id, "new-id");
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

START_TEST(test_map_status_incomplete_max_tokens) {
    ik_finish_reason_t reason = ik_openai_map_responses_status("incomplete", "max_output_tokens");
    ck_assert_int_eq(reason, IK_FINISH_LENGTH);
}

END_TEST

START_TEST(test_parse_usage_tokens_null) {
    const char *json = "{"
                       "\"id\":\"resp-usage\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[],"
                       "\"usage\":{"
                       "\"prompt_tokens\":null,"
                       "\"completion_tokens\":null,"
                       "\"total_tokens\":null"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->usage.input_tokens, 0);
    ck_assert_int_eq(resp->usage.output_tokens, 0);
    ck_assert_int_eq(resp->usage.total_tokens, 0);
}

END_TEST

START_TEST(test_parse_usage_reasoning_tokens_null) {
    const char *json = "{"
                       "\"id\":\"resp-usage\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":10,"
                       "\"total_tokens\":15,"
                       "\"completion_tokens_details\":{"
                       "\"reasoning_tokens\":null"
                       "}"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->usage.thinking_tokens, 0);
}

END_TEST

START_TEST(test_parse_function_call_name_null) {
    const char *json = "{"
                       "\"id\":\"resp-func\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"function_call\","
                       "\"id\":\"test-id\","
                       "\"name\":null,"
                       "\"arguments\":\"{}\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
}

END_TEST

START_TEST(test_parse_function_call_arguments_null) {
    const char *json = "{"
                       "\"id\":\"resp-func\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"function_call\","
                       "\"id\":\"test-id\","
                       "\"name\":\"test_func\","
                       "\"arguments\":null"
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *response_responses_coverage_suite(void)
{
    Suite *s = suite_create("OpenAI Responses API Coverage Tests");

    TCase *tc_usage = tcase_create("Usage Parsing Coverage");
    tcase_set_timeout(tc_usage, 30);
    tcase_add_checked_fixture(tc_usage, setup, teardown);
    tcase_add_test(tc_usage, test_parse_usage_prompt_tokens_not_int);
    tcase_add_test(tc_usage, test_parse_usage_completion_tokens_not_int);
    tcase_add_test(tc_usage, test_parse_usage_total_tokens_not_int);
    tcase_add_test(tc_usage, test_parse_usage_reasoning_tokens_not_int);
    tcase_add_test(tc_usage, test_parse_usage_tokens_null);
    tcase_add_test(tc_usage, test_parse_usage_reasoning_tokens_null);
    suite_add_tcase(s, tc_usage);

    TCase *tc_function = tcase_create("Function Call Coverage");
    tcase_set_timeout(tc_function, 30);
    tcase_add_checked_fixture(tc_function, setup, teardown);
    tcase_add_test(tc_function, test_parse_function_call_id_null);
    tcase_add_test(tc_function, test_parse_function_call_name_null);
    tcase_add_test(tc_function, test_parse_function_call_arguments_null);
    suite_add_tcase(s, tc_function);

    TCase *tc_count = tcase_create("Count Blocks Coverage");
    tcase_set_timeout(tc_count, 30);
    tcase_add_checked_fixture(tc_count, setup, teardown);
    tcase_add_test(tc_count, test_count_content_blocks_type_null);
    tcase_add_test(tc_count, test_count_content_blocks_type_not_string);
    suite_add_tcase(s, tc_count);

    TCase *tc_main = tcase_create("Main Parsing Coverage");
    tcase_set_timeout(tc_main, 30);
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
    suite_add_tcase(s, tc_main);

    TCase *tc_status = tcase_create("Status Mapping Coverage");
    tcase_set_timeout(tc_status, 30);
    tcase_add_test(tc_status, test_map_status_null);
    tcase_add_test(tc_status, test_map_status_failed);
    tcase_add_test(tc_status, test_map_status_cancelled);
    tcase_add_test(tc_status, test_map_status_incomplete_content_filter);
    tcase_add_test(tc_status, test_map_status_incomplete_max_tokens);
    suite_add_tcase(s, tc_status);

    TCase *tc_call_id = tcase_create("Call ID Coverage");
    tcase_set_timeout(tc_call_id, 30);
    tcase_add_checked_fixture(tc_call_id, setup, teardown);
    tcase_add_test(tc_call_id, test_parse_function_call_with_call_id);
    suite_add_tcase(s, tc_call_id);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = response_responses_coverage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
