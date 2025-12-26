/**
 * @file response_responses_simple_test.c
 * @brief Tests for OpenAI Responses API simple response parsing
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
 * Simple Response Parsing Tests
 * ================================================================ */

START_TEST(test_parse_simple_text_response)
{
    const char *json = "{"
                       "\"id\":\"resp-123\","
                       "\"object\":\"response\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":\"output_text\","
                       "\"text\":\"Hello there, how may I assist you today?\""
                       "}]"
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":9,"
                       "\"completion_tokens\":12,"
                       "\"total_tokens\":21"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_str_eq(resp->model, "gpt-4o");
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

START_TEST(test_parse_response_with_reasoning_tokens)
{
    const char *json = "{"
                       "\"id\":\"resp-456\","
                       "\"model\":\"o1-preview\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":\"output_text\","
                       "\"text\":\"After analysis, the answer is 42.\""
                       "}]"
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
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->usage.input_tokens, 50);
    ck_assert_int_eq(resp->usage.output_tokens, 15);
    ck_assert_int_eq(resp->usage.total_tokens, 65);
    ck_assert_int_eq(resp->usage.thinking_tokens, 25);
}

END_TEST

START_TEST(test_parse_response_with_refusal)
{
    const char *json = "{"
                       "\"id\":\"resp-789\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":\"refusal\","
                       "\"refusal\":\"I cannot help with that request.\""
                       "}]"
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":10,"
                       "\"completion_tokens\":8,"
                       "\"total_tokens\":18"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 1);
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(resp->content_blocks[0].data.text.text,
                     "I cannot help with that request.");
}

END_TEST

START_TEST(test_parse_response_multiple_content_blocks)
{
    const char *json = "{"
                       "\"id\":\"resp-multi\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":\"output_text\","
                       "\"text\":\"First block\""
                       "},{"
                       "\"type\":\"output_text\","
                       "\"text\":\"Second block\""
                       "}]"
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":6,"
                       "\"total_tokens\":11"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 2);
    ck_assert_str_eq(resp->content_blocks[0].data.text.text, "First block");
    ck_assert_str_eq(resp->content_blocks[1].data.text.text, "Second block");
}

END_TEST

START_TEST(test_parse_response_function_call)
{
    const char *json = "{"
                       "\"id\":\"resp-tool\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"function_call\","
                       "\"id\":\"call_abc123\","
                       "\"name\":\"get_weather\","
                       "\"arguments\":\"{\\\"location\\\":\\\"Boston\\\"}\""
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":20,"
                       "\"completion_tokens\":10,"
                       "\"total_tokens\":30"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 1);
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.id, "call_abc123");
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.name, "get_weather");
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.arguments, "{\"location\":\"Boston\"}");
}

END_TEST

START_TEST(test_parse_response_function_call_with_call_id)
{
    const char *json = "{"
                       "\"id\":\"resp-tool2\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"function_call\","
                       "\"id\":\"old_id\","
                       "\"call_id\":\"call_xyz789\","
                       "\"name\":\"get_time\","
                       "\"arguments\":\"{}\""
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":15,"
                       "\"completion_tokens\":5,"
                       "\"total_tokens\":20"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 1);
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.id, "call_xyz789");
}

END_TEST

START_TEST(test_parse_response_mixed_message_and_tool)
{
    const char *json = "{"
                       "\"id\":\"resp-mixed\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":\"output_text\","
                       "\"text\":\"Let me check that.\""
                       "}]"
                       "},{"
                       "\"type\":\"function_call\","
                       "\"id\":\"call_def456\","
                       "\"name\":\"search\","
                       "\"arguments\":\"{\\\"query\\\":\\\"test\\\"}\""
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":25,"
                       "\"completion_tokens\":15,"
                       "\"total_tokens\":40"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 2);
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_TEXT);
    ck_assert_int_eq(resp->content_blocks[1].type, IK_CONTENT_TOOL_CALL);
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *response_responses_simple_suite(void)
{
    Suite *s = suite_create("OpenAI Responses API Simple Response Parsing");

    TCase *tc_simple = tcase_create("Simple Responses");
    tcase_add_checked_fixture(tc_simple, setup, teardown);
    tcase_add_test(tc_simple, test_parse_simple_text_response);
    tcase_add_test(tc_simple, test_parse_response_with_reasoning_tokens);
    tcase_add_test(tc_simple, test_parse_response_with_refusal);
    tcase_add_test(tc_simple, test_parse_response_multiple_content_blocks);
    suite_add_tcase(s, tc_simple);

    TCase *tc_tools = tcase_create("Tool Call Responses");
    tcase_add_checked_fixture(tc_tools, setup, teardown);
    tcase_add_test(tc_tools, test_parse_response_function_call);
    tcase_add_test(tc_tools, test_parse_response_function_call_with_call_id);
    tcase_add_test(tc_tools, test_parse_response_mixed_message_and_tool);
    suite_add_tcase(s, tc_tools);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = response_responses_simple_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
