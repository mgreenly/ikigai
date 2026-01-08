#include "../../../test_constants.h"
/**
 * @file response_chat_test.c
 * @brief Tests for OpenAI Chat Completions response parsing
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
 * Finish Reason Mapping Tests
 * ================================================================ */

START_TEST(test_map_finish_reason_stop) {
    ik_finish_reason_t reason = ik_openai_map_chat_finish_reason("stop");
    ck_assert_int_eq(reason, IK_FINISH_STOP);
}
END_TEST

START_TEST(test_map_finish_reason_length) {
    ik_finish_reason_t reason = ik_openai_map_chat_finish_reason("length");
    ck_assert_int_eq(reason, IK_FINISH_LENGTH);
}

END_TEST

START_TEST(test_map_finish_reason_tool_calls) {
    ik_finish_reason_t reason = ik_openai_map_chat_finish_reason("tool_calls");
    ck_assert_int_eq(reason, IK_FINISH_TOOL_USE);
}

END_TEST

START_TEST(test_map_finish_reason_content_filter) {
    ik_finish_reason_t reason = ik_openai_map_chat_finish_reason("content_filter");
    ck_assert_int_eq(reason, IK_FINISH_CONTENT_FILTER);
}

END_TEST

START_TEST(test_map_finish_reason_error) {
    ik_finish_reason_t reason = ik_openai_map_chat_finish_reason("error");
    ck_assert_int_eq(reason, IK_FINISH_ERROR);
}

END_TEST

START_TEST(test_map_finish_reason_null) {
    ik_finish_reason_t reason = ik_openai_map_chat_finish_reason(NULL);
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}

END_TEST

START_TEST(test_map_finish_reason_unknown) {
    ik_finish_reason_t reason = ik_openai_map_chat_finish_reason("unknown_reason");
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}

END_TEST
/* ================================================================
 * Simple Response Parsing Tests
 * ================================================================ */

START_TEST(test_parse_simple_text_response) {
    const char *json = "{"
                       "\"id\":\"chatcmpl-123\","
                       "\"object\":\"chat.completion\","
                       "\"model\":\"gpt-4\","
                       "\"choices\":[{"
                       "\"index\":0,"
                       "\"message\":{"
                       "\"role\":\"assistant\","
                       "\"content\":\"Hello there, how may I assist you today?\""
                       "},"
                       "\"finish_reason\":\"stop\""
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":9,"
                       "\"completion_tokens\":12,"
                       "\"total_tokens\":21"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_str_eq(resp->model, "gpt-4");
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_STOP);
    ck_assert_int_eq((int)resp->content_count, 1);
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(resp->content_blocks[0].data.text.text,
                     "Hello there, how may I assist you today?");
    ck_assert_int_eq(resp->usage.input_tokens, 9);
    ck_assert_int_eq(resp->usage.output_tokens, 12);
    ck_assert_int_eq(resp->usage.total_tokens, 21);
    ck_assert_int_eq(resp->usage.thinking_tokens, 0);
}

END_TEST

START_TEST(test_parse_response_with_reasoning_tokens) {
    const char *json = "{"
                       "\"id\":\"chatcmpl-456\","
                       "\"model\":\"o1-preview\","
                       "\"choices\":[{"
                       "\"index\":0,"
                       "\"message\":{"
                       "\"role\":\"assistant\","
                       "\"content\":\"After careful analysis, the answer is 42.\""
                       "},"
                       "\"finish_reason\":\"stop\""
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":50,"
                       "\"completion_tokens\":15,"
                       "\"total_tokens\":65,"
                       "\"completion_tokens_details\":{"
                       "\"reasoning_tokens\":25"
                       "}"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->usage.input_tokens, 50);
    ck_assert_int_eq(resp->usage.output_tokens, 15);
    ck_assert_int_eq(resp->usage.total_tokens, 65);
    ck_assert_int_eq(resp->usage.thinking_tokens, 25);
}

END_TEST

START_TEST(test_parse_response_no_usage) {
    const char *json = "{"
                       "\"id\":\"chatcmpl-123\","
                       "\"model\":\"gpt-4\","
                       "\"choices\":[{"
                       "\"index\":0,"
                       "\"message\":{"
                       "\"role\":\"assistant\","
                       "\"content\":\"Hello\""
                       "},"
                       "\"finish_reason\":\"stop\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->usage.input_tokens, 0);
    ck_assert_int_eq(resp->usage.output_tokens, 0);
    ck_assert_int_eq(resp->usage.total_tokens, 0);
    ck_assert_int_eq(resp->usage.thinking_tokens, 0);
}

END_TEST

START_TEST(test_parse_response_null_content) {
    const char *json = "{"
                       "\"id\":\"chatcmpl-123\","
                       "\"model\":\"gpt-4\","
                       "\"choices\":[{"
                       "\"index\":0,"
                       "\"message\":{"
                       "\"role\":\"assistant\","
                       "\"content\":null"
                       "},"
                       "\"finish_reason\":\"stop\""
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":0,"
                       "\"total_tokens\":5"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
    ck_assert_ptr_null(resp->content_blocks);
}

END_TEST

START_TEST(test_parse_response_empty_content) {
    const char *json = "{"
                       "\"id\":\"chatcmpl-123\","
                       "\"model\":\"gpt-4\","
                       "\"choices\":[{"
                       "\"index\":0,"
                       "\"message\":{"
                       "\"role\":\"assistant\","
                       "\"content\":\"\""
                       "},"
                       "\"finish_reason\":\"stop\""
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":0,"
                       "\"total_tokens\":5"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
    ck_assert_ptr_null(resp->content_blocks);
}

END_TEST
/* ================================================================
 * Tool Call Response Tests
 * ================================================================ */

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *response_chat_suite(void)
{
    Suite *s = suite_create("OpenAI Chat Response Parsing - Basic");

    TCase *tc_finish_reason = tcase_create("Finish Reason Mapping");
    tcase_set_timeout(tc_finish_reason, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_finish_reason, setup, teardown);
    tcase_add_test(tc_finish_reason, test_map_finish_reason_stop);
    tcase_add_test(tc_finish_reason, test_map_finish_reason_length);
    tcase_add_test(tc_finish_reason, test_map_finish_reason_tool_calls);
    tcase_add_test(tc_finish_reason, test_map_finish_reason_content_filter);
    tcase_add_test(tc_finish_reason, test_map_finish_reason_error);
    tcase_add_test(tc_finish_reason, test_map_finish_reason_null);
    tcase_add_test(tc_finish_reason, test_map_finish_reason_unknown);
    suite_add_tcase(s, tc_finish_reason);

    TCase *tc_simple = tcase_create("Simple Responses");
    tcase_set_timeout(tc_simple, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_simple, setup, teardown);
    tcase_add_test(tc_simple, test_parse_simple_text_response);
    tcase_add_test(tc_simple, test_parse_response_with_reasoning_tokens);
    tcase_add_test(tc_simple, test_parse_response_no_usage);
    tcase_add_test(tc_simple, test_parse_response_null_content);
    tcase_add_test(tc_simple, test_parse_response_empty_content);
    suite_add_tcase(s, tc_simple);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = response_chat_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
