/**
 * @file response_responses_edge2a_test.c
 * @brief Tests for OpenAI Responses API edge cases - invalid types (part 1)
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
 * Edge Cases - Invalid Types (Part 1)
 * ================================================================ */

START_TEST(test_parse_response_skip_content_no_type) {
    const char *json = "{"
                       "\"id\":\"resp-skiptype\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"text\":\"no type field\""
                       "},{"
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

START_TEST(test_parse_response_message_no_content_array) {
    const char *json = "{"
                       "\"id\":\"resp-nocontent\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\""
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

START_TEST(test_parse_response_message_content_not_array) {
    const char *json = "{"
                       "\"id\":\"resp-contentnotarr\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":\"not an array\""
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

START_TEST(test_parse_response_skip_content_type_not_string) {
    const char *json = "{"
                       "\"id\":\"resp-typenotstr\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":456,"
                       "\"text\":\"bad type\""
                       "},{"
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

START_TEST(test_parse_response_skip_unknown_content_type) {
    const char *json = "{"
                       "\"id\":\"resp-unknownc\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":\"unknown_content\","
                       "\"data\":\"some data\""
                       "},{"
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

START_TEST(test_parse_response_output_text_no_text_field) {
    const char *json = "{"
                       "\"id\":\"resp-notext\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":\"output_text\""
                       "}]"
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

START_TEST(test_parse_response_output_text_text_not_string) {
    const char *json = "{"
                       "\"id\":\"resp-textnotstr\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":\"output_text\","
                       "\"text\":123"
                       "}]"
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

static Suite *response_responses_edge2a_suite(void)
{
    Suite *s = suite_create("OpenAI Responses API Edge Cases (Invalid Types Part 1)");

    TCase *tc_edge = tcase_create("Invalid Types Part 1");
    tcase_set_timeout(tc_edge, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_edge, setup, teardown);
    tcase_add_test(tc_edge, test_parse_response_skip_content_no_type);
    tcase_add_test(tc_edge, test_parse_response_message_no_content_array);
    tcase_add_test(tc_edge, test_parse_response_message_content_not_array);
    tcase_add_test(tc_edge, test_parse_response_skip_content_type_not_string);
    tcase_add_test(tc_edge, test_parse_response_skip_unknown_content_type);
    tcase_add_test(tc_edge, test_parse_response_output_text_no_text_field);
    tcase_add_test(tc_edge, test_parse_response_output_text_text_not_string);
    suite_add_tcase(s, tc_edge);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = response_responses_edge2a_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
