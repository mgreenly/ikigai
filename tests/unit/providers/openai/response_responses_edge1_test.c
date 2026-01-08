/**
 * @file response_responses_edge1_test.c
 * @brief Tests for OpenAI Responses API edge cases - missing fields
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
 * Edge Cases - Missing Fields
 * ================================================================ */

START_TEST(test_parse_response_no_model) {
    const char *json = "{"
                       "\"id\":\"resp-nomodel\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":\"output_text\","
                       "\"text\":\"Hello\""
                       "}]"
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":2,"
                       "\"total_tokens\":7"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_ptr_null(resp->model);
}

END_TEST

START_TEST(test_parse_response_no_usage) {
    const char *json = "{"
                       "\"id\":\"resp-nousage\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
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
    ck_assert_int_eq(resp->usage.input_tokens, 0);
    ck_assert_int_eq(resp->usage.output_tokens, 0);
    ck_assert_int_eq(resp->usage.total_tokens, 0);
    ck_assert_int_eq(resp->usage.thinking_tokens, 0);
}

END_TEST

START_TEST(test_parse_response_no_status) {
    const char *json = "{"
                       "\"id\":\"resp-nostatus\","
                       "\"model\":\"gpt-4o\","
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":\"output_text\","
                       "\"text\":\"Hello\""
                       "}]"
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":2,"
                       "\"total_tokens\":7"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_UNKNOWN);
}

END_TEST

START_TEST(test_parse_response_no_output) {
    const char *json = "{"
                       "\"id\":\"resp-nooutput\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":0,"
                       "\"total_tokens\":5"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_ptr_null(resp->content_blocks);
    ck_assert_int_eq((int)resp->content_count, 0);
}

END_TEST

START_TEST(test_parse_response_empty_output_array) {
    const char *json = "{"
                       "\"id\":\"resp-empty\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":0,"
                       "\"total_tokens\":5"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_ptr_null(resp->content_blocks);
    ck_assert_int_eq((int)resp->content_count, 0);
}

END_TEST

START_TEST(test_parse_response_output_not_array) {
    const char *json = "{"
                       "\"id\":\"resp-badoutput\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":\"not an array\","
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":0,"
                       "\"total_tokens\":5"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_ptr_null(resp->content_blocks);
    ck_assert_int_eq((int)resp->content_count, 0);
}

END_TEST

START_TEST(test_parse_response_incomplete_with_details) {
    const char *json = "{"
                       "\"id\":\"resp-incomplete\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"incomplete\","
                       "\"incomplete_details\":{"
                       "\"reason\":\"max_output_tokens\""
                       "},"
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":\"output_text\","
                       "\"text\":\"Partial response\""
                       "}]"
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":100,"
                       "\"completion_tokens\":200,"
                       "\"total_tokens\":300"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_LENGTH);
}

END_TEST

START_TEST(test_parse_response_skip_unknown_output_type) {
    const char *json = "{"
                       "\"id\":\"resp-unknown\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"unknown_type\","
                       "\"data\":\"some data\""
                       "},{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":\"output_text\","
                       "\"text\":\"Valid text\""
                       "}]"
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":2,"
                       "\"total_tokens\":7"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 1);
    ck_assert_str_eq(resp->content_blocks[0].data.text.text, "Valid text");
}

END_TEST

START_TEST(test_parse_response_skip_item_missing_type) {
    const char *json = "{"
                       "\"id\":\"resp-notype\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"data\":\"no type field\""
                       "},{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":\"output_text\","
                       "\"text\":\"Valid text\""
                       "}]"
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":2,"
                       "\"total_tokens\":7"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 1);
}

END_TEST

START_TEST(test_parse_response_skip_item_type_not_string) {
    const char *json = "{"
                       "\"id\":\"resp-typenum\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":123"
                       "},{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":\"output_text\","
                       "\"text\":\"Valid text\""
                       "}]"
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":2,"
                       "\"total_tokens\":7"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 1);
}

END_TEST

START_TEST(test_parse_response_message_no_content) {
    const char *json = "{"
                       "\"id\":\"resp-nocontent\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\""
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":0,"
                       "\"total_tokens\":5"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
}

END_TEST

START_TEST(test_parse_response_message_content_not_array) {
    const char *json = "{"
                       "\"id\":\"resp-contentbad\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":\"not an array\""
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":0,"
                       "\"total_tokens\":5"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *response_responses_edge1_suite(void)
{
    Suite *s = suite_create("OpenAI Responses API Edge Cases (Missing Fields)");

    TCase *tc_edge = tcase_create("Missing Fields");
    tcase_set_timeout(tc_edge, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_edge, setup, teardown);
    tcase_add_test(tc_edge, test_parse_response_no_model);
    tcase_add_test(tc_edge, test_parse_response_no_usage);
    tcase_add_test(tc_edge, test_parse_response_no_status);
    tcase_add_test(tc_edge, test_parse_response_no_output);
    tcase_add_test(tc_edge, test_parse_response_empty_output_array);
    tcase_add_test(tc_edge, test_parse_response_output_not_array);
    tcase_add_test(tc_edge, test_parse_response_incomplete_with_details);
    tcase_add_test(tc_edge, test_parse_response_skip_unknown_output_type);
    tcase_add_test(tc_edge, test_parse_response_skip_item_missing_type);
    tcase_add_test(tc_edge, test_parse_response_skip_item_type_not_string);
    tcase_add_test(tc_edge, test_parse_response_message_no_content);
    tcase_add_test(tc_edge, test_parse_response_message_content_not_array);
    suite_add_tcase(s, tc_edge);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = response_responses_edge1_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
