/**
 * @file response_responses_test.c
 * @brief Tests for OpenAI Responses API response parsing
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
 * Status Mapping Tests
 * ================================================================ */

START_TEST(test_map_responses_status_null)
{
    ik_finish_reason_t reason = ik_openai_map_responses_status(NULL, NULL);
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}

END_TEST

START_TEST(test_map_responses_status_completed)
{
    ik_finish_reason_t reason = ik_openai_map_responses_status("completed", NULL);
    ck_assert_int_eq(reason, IK_FINISH_STOP);
}

END_TEST

START_TEST(test_map_responses_status_failed)
{
    ik_finish_reason_t reason = ik_openai_map_responses_status("failed", NULL);
    ck_assert_int_eq(reason, IK_FINISH_ERROR);
}

END_TEST

START_TEST(test_map_responses_status_cancelled)
{
    ik_finish_reason_t reason = ik_openai_map_responses_status("cancelled", NULL);
    ck_assert_int_eq(reason, IK_FINISH_STOP);
}

END_TEST

START_TEST(test_map_responses_status_incomplete_max_tokens)
{
    ik_finish_reason_t reason = ik_openai_map_responses_status("incomplete", "max_output_tokens");
    ck_assert_int_eq(reason, IK_FINISH_LENGTH);
}

END_TEST

START_TEST(test_map_responses_status_incomplete_content_filter)
{
    ik_finish_reason_t reason = ik_openai_map_responses_status("incomplete", "content_filter");
    ck_assert_int_eq(reason, IK_FINISH_CONTENT_FILTER);
}

END_TEST

START_TEST(test_map_responses_status_incomplete_null_reason)
{
    ik_finish_reason_t reason = ik_openai_map_responses_status("incomplete", NULL);
    ck_assert_int_eq(reason, IK_FINISH_LENGTH);
}

END_TEST

START_TEST(test_map_responses_status_incomplete_unknown_reason)
{
    ik_finish_reason_t reason = ik_openai_map_responses_status("incomplete", "other_reason");
    ck_assert_int_eq(reason, IK_FINISH_LENGTH);
}

END_TEST

START_TEST(test_map_responses_status_unknown)
{
    ik_finish_reason_t reason = ik_openai_map_responses_status("unknown_status", NULL);
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}

END_TEST

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
 * Edge Cases and Missing Fields
 * ================================================================ */

START_TEST(test_parse_response_no_model)
{
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

START_TEST(test_parse_response_no_usage)
{
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

START_TEST(test_parse_response_no_status)
{
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

START_TEST(test_parse_response_no_output)
{
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

START_TEST(test_parse_response_empty_output_array)
{
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

START_TEST(test_parse_response_output_not_array)
{
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

START_TEST(test_parse_response_incomplete_with_details)
{
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

START_TEST(test_parse_response_skip_unknown_output_type)
{
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

START_TEST(test_parse_response_skip_item_missing_type)
{
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

START_TEST(test_parse_response_skip_item_type_not_string)
{
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

START_TEST(test_parse_response_message_no_content)
{
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

START_TEST(test_parse_response_message_content_not_array)
{
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

START_TEST(test_parse_response_skip_content_no_type)
{
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

START_TEST(test_parse_response_skip_content_type_not_string)
{
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

START_TEST(test_parse_response_skip_unknown_content_type)
{
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

START_TEST(test_parse_response_output_text_no_text_field)
{
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

START_TEST(test_parse_response_output_text_text_not_string)
{
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

START_TEST(test_parse_response_refusal_no_field)
{
    const char *json = "{"
                       "\"id\":\"resp-norefusal\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":\"refusal\""
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

START_TEST(test_parse_response_refusal_not_string)
{
    const char *json = "{"
                       "\"id\":\"resp-refusalnotstr\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":\"refusal\","
                       "\"refusal\":789"
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

START_TEST(test_parse_response_usage_non_int_values)
{
    const char *json = "{"
                       "\"id\":\"resp-badusage\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"message\","
                       "\"content\":[{"
                       "\"type\":\"output_text\","
                       "\"text\":\"Hello\""
                       "}]"
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":\"not_an_int\","
                       "\"completion_tokens\":true,"
                       "\"total_tokens\":null,"
                       "\"completion_tokens_details\":{"
                       "\"reasoning_tokens\":\"also_not_int\""
                       "}"
                       "}"
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

START_TEST(test_parse_response_model_not_string)
{
    const char *json = "{"
                       "\"id\":\"resp-modelnum\","
                       "\"model\":123,"
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

START_TEST(test_parse_response_status_not_string)
{
    const char *json = "{"
                       "\"id\":\"resp-statusnum\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":999,"
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

START_TEST(test_parse_response_incomplete_reason_not_string)
{
    const char *json = "{"
                       "\"id\":\"resp-reasonnum\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"incomplete\","
                       "\"incomplete_details\":{"
                       "\"reason\":456"
                       "},"
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
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_LENGTH);
}

END_TEST

START_TEST(test_parse_response_function_call_call_id_not_string)
{
    const char *json = "{"
                       "\"id\":\"resp-callidnotstr\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"function_call\","
                       "\"id\":\"old_id\","
                       "\"call_id\":789,"
                       "\"name\":\"get_weather\","
                       "\"arguments\":\"{}\""
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
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.id, "old_id");
}

END_TEST

/* ================================================================
 * Error Cases
 * ================================================================ */

START_TEST(test_parse_response_invalid_json)
{
    const char *json = "{invalid json}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_ptr_null(resp);
}

END_TEST

START_TEST(test_parse_response_not_object)
{
    const char *json = "[\"array\", \"not\", \"object\"]";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_ptr_null(resp);
}

END_TEST

START_TEST(test_parse_response_error_response)
{
    const char *json = "{"
                       "\"error\":{"
                       "\"message\":\"Invalid API key\","
                       "\"type\":\"invalid_request_error\","
                       "\"code\":\"invalid_api_key\""
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_ptr_null(resp);
}

END_TEST

START_TEST(test_parse_response_error_no_message)
{
    const char *json = "{"
                       "\"error\":{"
                       "\"type\":\"error_type\""
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_ptr_null(resp);
}

END_TEST

START_TEST(test_parse_response_error_message_not_string)
{
    const char *json = "{"
                       "\"error\":{"
                       "\"message\":123"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_ptr_null(resp);
}

END_TEST

START_TEST(test_parse_response_function_call_no_id)
{
    const char *json = "{"
                       "\"id\":\"resp-noid\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"function_call\","
                       "\"name\":\"get_weather\","
                       "\"arguments\":\"{}\""
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":2,"
                       "\"total_tokens\":7"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
}

END_TEST

START_TEST(test_parse_response_function_call_id_not_string)
{
    const char *json = "{"
                       "\"id\":\"resp-idnotstr\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"function_call\","
                       "\"id\":999,"
                       "\"name\":\"get_weather\","
                       "\"arguments\":\"{}\""
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":2,"
                       "\"total_tokens\":7"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
}

END_TEST

START_TEST(test_parse_response_function_call_no_name)
{
    const char *json = "{"
                       "\"id\":\"resp-noname\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"function_call\","
                       "\"id\":\"call_123\","
                       "\"arguments\":\"{}\""
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":2,"
                       "\"total_tokens\":7"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
}

END_TEST

START_TEST(test_parse_response_function_call_name_not_string)
{
    const char *json = "{"
                       "\"id\":\"resp-namenotstr\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"function_call\","
                       "\"id\":\"call_123\","
                       "\"name\":456,"
                       "\"arguments\":\"{}\""
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":2,"
                       "\"total_tokens\":7"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
}

END_TEST

START_TEST(test_parse_response_function_call_no_arguments)
{
    const char *json = "{"
                       "\"id\":\"resp-noargs\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"function_call\","
                       "\"id\":\"call_123\","
                       "\"name\":\"get_weather\""
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":2,"
                       "\"total_tokens\":7"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
}

END_TEST

START_TEST(test_parse_response_function_call_arguments_not_string)
{
    const char *json = "{"
                       "\"id\":\"resp-argsnotstr\","
                       "\"model\":\"gpt-4o\","
                       "\"status\":\"completed\","
                       "\"output\":[{"
                       "\"type\":\"function_call\","
                       "\"id\":\"call_123\","
                       "\"name\":\"get_weather\","
                       "\"arguments\":123"
                       "}],"
                       "\"usage\":{"
                       "\"prompt_tokens\":5,"
                       "\"completion_tokens\":2,"
                       "\"total_tokens\":7"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *response_responses_suite(void)
{
    Suite *s = suite_create("OpenAI Responses API Response Parsing");

    TCase *tc_status = tcase_create("Status Mapping");
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

    TCase *tc_edge = tcase_create("Edge Cases");
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
    tcase_add_test(tc_edge, test_parse_response_skip_content_no_type);
    tcase_add_test(tc_edge, test_parse_response_skip_content_type_not_string);
    tcase_add_test(tc_edge, test_parse_response_skip_unknown_content_type);
    tcase_add_test(tc_edge, test_parse_response_output_text_no_text_field);
    tcase_add_test(tc_edge, test_parse_response_output_text_text_not_string);
    tcase_add_test(tc_edge, test_parse_response_refusal_no_field);
    tcase_add_test(tc_edge, test_parse_response_refusal_not_string);
    tcase_add_test(tc_edge, test_parse_response_usage_non_int_values);
    tcase_add_test(tc_edge, test_parse_response_model_not_string);
    tcase_add_test(tc_edge, test_parse_response_status_not_string);
    tcase_add_test(tc_edge, test_parse_response_incomplete_reason_not_string);
    tcase_add_test(tc_edge, test_parse_response_function_call_call_id_not_string);
    suite_add_tcase(s, tc_edge);

    TCase *tc_errors = tcase_create("Error Cases");
    tcase_add_checked_fixture(tc_errors, setup, teardown);
    tcase_add_test(tc_errors, test_parse_response_invalid_json);
    tcase_add_test(tc_errors, test_parse_response_not_object);
    tcase_add_test(tc_errors, test_parse_response_error_response);
    tcase_add_test(tc_errors, test_parse_response_error_no_message);
    tcase_add_test(tc_errors, test_parse_response_error_message_not_string);
    tcase_add_test(tc_errors, test_parse_response_function_call_no_id);
    tcase_add_test(tc_errors, test_parse_response_function_call_id_not_string);
    tcase_add_test(tc_errors, test_parse_response_function_call_no_name);
    tcase_add_test(tc_errors, test_parse_response_function_call_name_not_string);
    tcase_add_test(tc_errors, test_parse_response_function_call_no_arguments);
    tcase_add_test(tc_errors, test_parse_response_function_call_arguments_not_string);
    suite_add_tcase(s, tc_errors);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = response_responses_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
