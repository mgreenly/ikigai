/**
 * @file response_responses_content_test.c
 * @brief Tests for OpenAI Responses API content parsing coverage
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

START_TEST(test_count_content_blocks_unknown_type) {
    const char *json = "{"
                       "\"id\":\"resp-unknown\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"unknown_type\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
}

END_TEST

START_TEST(test_count_content_blocks_message_no_content) {
    const char *json = "{"
                       "\"id\":\"resp-message\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
}

END_TEST

START_TEST(test_count_content_blocks_no_type) {
    const char *json = "{"
                       "\"id\":\"resp-no-type\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{\"foo\":\"bar\"}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
}

END_TEST

/* ================================================================
 * Coverage Tests for parse_content
 * ================================================================ */

START_TEST(test_parse_content_message_content_null) {
    const char *json = "{"
                       "\"id\":\"resp-content\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":null"
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
}

END_TEST

START_TEST(test_parse_content_message_content_not_array) {
    const char *json = "{"
                       "\"id\":\"resp-content\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":\"not an array\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
}

END_TEST

START_TEST(test_parse_content_item_type_null) {
    const char *json = "{"
                       "\"id\":\"resp-content\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":null,"
                       "\"text\":\"Hello\""
                       "}]"
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
}

END_TEST

START_TEST(test_parse_content_item_type_not_string) {
    const char *json = "{"
                       "\"id\":\"resp-content\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":123,"
                       "\"text\":\"Hello\""
                       "}]"
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
}

END_TEST

START_TEST(test_parse_content_output_text_null) {
    const char *json = "{"
                       "\"id\":\"resp-content\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":\"output_text\","
                       "\"text\":null"
                       "}]"
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
}

END_TEST

START_TEST(test_parse_content_refusal_null) {
    const char *json = "{"
                       "\"id\":\"resp-content\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":\"refusal\","
                       "\"refusal\":null"
                       "}]"
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
}

END_TEST

START_TEST(test_parse_content_refusal_valid) {
    const char *json = "{"
                       "\"id\":\"resp-content\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":\"refusal\","
                       "\"refusal\":\"I cannot help with that\""
                       "}]"
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 1);
    ck_assert_str_eq(resp->content_blocks[0].data.text.text, "I cannot help with that");
}

END_TEST

START_TEST(test_parse_content_text_val_null) {
    const char *json = "{"
                       "\"id\":\"resp-content\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":\"output_text\""
                       "}]"
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
}

END_TEST

START_TEST(test_parse_content_refusal_val_null) {
    const char *json = "{"
                       "\"id\":\"resp-content\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":\"refusal\""
                       "}]"
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
}

END_TEST

START_TEST(test_parse_content_unknown_content_type) {
    const char *json = "{"
                       "\"id\":\"resp-content\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":\"unknown_content_type\","
                       "\"data\":\"some data\""
                       "}]"
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
}

END_TEST

START_TEST(test_parse_message_no_type) {
    const char *json = "{"
                       "\"id\":\"resp-message\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
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
    ck_assert_int_eq((int)resp->content_count, 0);
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *response_responses_content_suite(void)
{
    Suite *s = suite_create("OpenAI Responses API Content Parsing Tests");

    TCase *tc_count = tcase_create("Count Blocks Coverage");
    tcase_set_timeout(tc_count, 30);
    tcase_add_checked_fixture(tc_count, setup, teardown);
    tcase_add_test(tc_count, test_count_content_blocks_type_null);
    tcase_add_test(tc_count, test_count_content_blocks_type_not_string);
    tcase_add_test(tc_count, test_count_content_blocks_unknown_type);
    tcase_add_test(tc_count, test_count_content_blocks_message_no_content);
    tcase_add_test(tc_count, test_count_content_blocks_no_type);
    suite_add_tcase(s, tc_count);

    TCase *tc_content = tcase_create("Content Parsing Coverage");
    tcase_set_timeout(tc_content, 30);
    tcase_add_checked_fixture(tc_content, setup, teardown);
    tcase_add_test(tc_content, test_parse_content_message_content_null);
    tcase_add_test(tc_content, test_parse_content_message_content_not_array);
    tcase_add_test(tc_content, test_parse_content_item_type_null);
    tcase_add_test(tc_content, test_parse_content_item_type_not_string);
    tcase_add_test(tc_content, test_parse_content_output_text_null);
    tcase_add_test(tc_content, test_parse_content_refusal_null);
    tcase_add_test(tc_content, test_parse_content_refusal_valid);
    tcase_add_test(tc_content, test_parse_content_text_val_null);
    tcase_add_test(tc_content, test_parse_content_refusal_val_null);
    tcase_add_test(tc_content, test_parse_content_unknown_content_type);
    tcase_add_test(tc_content, test_parse_message_no_type);
    suite_add_tcase(s, tc_content);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = response_responses_content_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
