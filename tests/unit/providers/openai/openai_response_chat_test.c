/**
 * @file openai_response_chat_test.c
 * @brief Unit tests for OpenAI chat response parsing functions
 *
 * Tests ik_openai_map_chat_finish_reason and ik_openai_parse_chat_response
 * to achieve 100% coverage of response_chat.c
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/openai/response.h"
#include "providers/provider.h"
#include "error.h"

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
 * ik_openai_map_chat_finish_reason Tests
 * ================================================================ */

START_TEST(test_map_finish_reason_null)
{
    // Test NULL finish_reason (line 125-126)
    ik_finish_reason_t result = ik_openai_map_chat_finish_reason(NULL);
    ck_assert_int_eq(result, IK_FINISH_UNKNOWN);
}
END_TEST

START_TEST(test_map_finish_reason_stop)
{
    // Test "stop" finish_reason (line 129-130)
    ik_finish_reason_t result = ik_openai_map_chat_finish_reason("stop");
    ck_assert_int_eq(result, IK_FINISH_STOP);
}
END_TEST

START_TEST(test_map_finish_reason_length)
{
    // Test "length" finish_reason (line 131-132)
    ik_finish_reason_t result = ik_openai_map_chat_finish_reason("length");
    ck_assert_int_eq(result, IK_FINISH_LENGTH);
}
END_TEST

START_TEST(test_map_finish_reason_tool_calls)
{
    // Test "tool_calls" finish_reason (line 133-134)
    ik_finish_reason_t result = ik_openai_map_chat_finish_reason("tool_calls");
    ck_assert_int_eq(result, IK_FINISH_TOOL_USE);
}
END_TEST

START_TEST(test_map_finish_reason_content_filter)
{
    // Test "content_filter" finish_reason (line 135-136)
    ik_finish_reason_t result = ik_openai_map_chat_finish_reason("content_filter");
    ck_assert_int_eq(result, IK_FINISH_CONTENT_FILTER);
}
END_TEST

START_TEST(test_map_finish_reason_error)
{
    // Test "error" finish_reason (line 137-138)
    ik_finish_reason_t result = ik_openai_map_chat_finish_reason("error");
    ck_assert_int_eq(result, IK_FINISH_ERROR);
}
END_TEST

START_TEST(test_map_finish_reason_unknown)
{
    // Test unknown finish_reason (line 141)
    ik_finish_reason_t result = ik_openai_map_chat_finish_reason("unknown_reason");
    ck_assert_int_eq(result, IK_FINISH_UNKNOWN);
}
END_TEST

/* ================================================================
 * ik_openai_parse_chat_response Tests
 * ================================================================ */

START_TEST(test_parse_chat_invalid_json)
{
    // Test invalid JSON (line 155-156)
    ik_response_t *resp = NULL;
    res_t r = ik_openai_parse_chat_response(test_ctx, "not valid json", 14, &resp);

    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_PARSE);
    ck_assert(strstr(r.err->msg, "Invalid JSON") != NULL);
}
END_TEST

START_TEST(test_parse_chat_not_object)
{
    // Test JSON that is not an object (line 160-161)
    const char *json = "[1, 2, 3]";
    ik_response_t *resp = NULL;
    res_t r = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_PARSE);
    ck_assert(strstr(r.err->msg, "not an object") != NULL);
}
END_TEST

START_TEST(test_parse_chat_error_response)
{
    // Test error response (line 166-176)
    const char *json =
        "{"
        "  \"error\": {"
        "    \"message\": \"Invalid API key\""
        "  }"
        "}";

    ik_response_t *resp = NULL;
    res_t r = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_PROVIDER);
    ck_assert(strstr(r.err->msg, "API error") != NULL);
}
END_TEST

START_TEST(test_parse_chat_error_response_no_message)
{
    // Test error response without message field (line 168-174)
    const char *json =
        "{"
        "  \"error\": {"
        "    \"type\": \"server_error\""
        "  }"
        "}";

    ik_response_t *resp = NULL;
    res_t r = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_PROVIDER);
    ck_assert(strstr(r.err->msg, "Unknown error") != NULL);
}
END_TEST

START_TEST(test_parse_chat_no_choices)
{
    // Test response with no choices field (line 199-205)
    const char *json =
        "{"
        "  \"model\": \"gpt-4\""
        "}";

    ik_response_t *resp = NULL;
    res_t r = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
    ck_assert_ptr_null(resp->content_blocks);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_UNKNOWN);
}
END_TEST

START_TEST(test_parse_chat_empty_choices)
{
    // Test response with empty choices array (line 209-215)
    const char *json =
        "{"
        "  \"model\": \"gpt-4\","
        "  \"choices\": []"
        "}";

    ik_response_t *resp = NULL;
    res_t r = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
    ck_assert_ptr_null(resp->content_blocks);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_UNKNOWN);
}
END_TEST

START_TEST(test_parse_chat_no_message)
{
    // Test response with choice but no message (line 238-242)
    const char *json =
        "{"
        "  \"model\": \"gpt-4\","
        "  \"choices\": ["
        "    {"
        "      \"finish_reason\": \"stop\","
        "      \"index\": 0"
        "    }"
        "  ]"
        "}";

    ik_response_t *resp = NULL;
    res_t r = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
    ck_assert_ptr_null(resp->content_blocks);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_STOP);
}
END_TEST

START_TEST(test_parse_chat_empty_content)
{
    // Test response with empty content (line 267-271)
    const char *json =
        "{"
        "  \"model\": \"gpt-4\","
        "  \"choices\": ["
        "    {"
        "      \"message\": {"
        "        \"role\": \"assistant\","
        "        \"content\": \"\""
        "      },"
        "      \"finish_reason\": \"stop\""
        "    }"
        "  ]"
        "}";

    ik_response_t *resp = NULL;
    res_t r = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
    ck_assert_ptr_null(resp->content_blocks);
}
END_TEST

START_TEST(test_parse_chat_null_content)
{
    // Test response with null content (line 251)
    const char *json =
        "{"
        "  \"model\": \"gpt-4\","
        "  \"choices\": ["
        "    {"
        "      \"message\": {"
        "        \"role\": \"assistant\","
        "        \"content\": null"
        "      },"
        "      \"finish_reason\": \"stop\""
        "    }"
        "  ]"
        "}";

    ik_response_t *resp = NULL;
    res_t r = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
    ck_assert_ptr_null(resp->content_blocks);
}
END_TEST

START_TEST(test_parse_chat_text_content)
{
    // Test response with text content (line 283-288)
    const char *json =
        "{"
        "  \"model\": \"gpt-4\","
        "  \"usage\": {"
        "    \"prompt_tokens\": 10,"
        "    \"completion_tokens\": 20,"
        "    \"total_tokens\": 30"
        "  },"
        "  \"choices\": ["
        "    {"
        "      \"message\": {"
        "        \"role\": \"assistant\","
        "        \"content\": \"Hello, world!\""
        "      },"
        "      \"finish_reason\": \"stop\""
        "    }"
        "  ]"
        "}";

    ik_response_t *resp = NULL;
    res_t r = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 1);
    ck_assert_ptr_nonnull(resp->content_blocks);
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(resp->content_blocks[0].data.text.text, "Hello, world!");
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_STOP);
    ck_assert_int_eq(resp->usage.input_tokens, 10);
    ck_assert_int_eq(resp->usage.output_tokens, 20);
    ck_assert_int_eq(resp->usage.total_tokens, 30);
}
END_TEST

START_TEST(test_parse_chat_usage_with_reasoning_tokens)
{
    // Test usage parsing with reasoning_tokens (line 110-116)
    const char *json =
        "{"
        "  \"model\": \"gpt-4\","
        "  \"usage\": {"
        "    \"prompt_tokens\": 10,"
        "    \"completion_tokens\": 20,"
        "    \"total_tokens\": 30,"
        "    \"completion_tokens_details\": {"
        "      \"reasoning_tokens\": 5"
        "    }"
        "  },"
        "  \"choices\": ["
        "    {"
        "      \"message\": {"
        "        \"role\": \"assistant\","
        "        \"content\": \"Test\""
        "      },"
        "      \"finish_reason\": \"stop\""
        "    }"
        "  ]"
        "}";

    ik_response_t *resp = NULL;
    res_t r = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->usage.input_tokens, 10);
    ck_assert_int_eq(resp->usage.output_tokens, 20);
    ck_assert_int_eq(resp->usage.total_tokens, 30);
    ck_assert_int_eq(resp->usage.thinking_tokens, 5);
}
END_TEST

START_TEST(test_parse_chat_tool_calls)
{
    // Test response with tool calls (line 292-301)
    const char *json =
        "{"
        "  \"model\": \"gpt-4\","
        "  \"choices\": ["
        "    {"
        "      \"message\": {"
        "        \"role\": \"assistant\","
        "        \"content\": null,"
        "        \"tool_calls\": ["
        "          {"
        "            \"id\": \"call_123\","
        "            \"function\": {"
        "              \"name\": \"get_weather\","
        "              \"arguments\": \"{\\\"location\\\":\\\"San Francisco\\\"}\""
        "            }"
        "          }"
        "        ]"
        "      },"
        "      \"finish_reason\": \"tool_calls\""
        "    }"
        "  ]"
        "}";

    ik_response_t *resp = NULL;
    res_t r = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 1);
    ck_assert_ptr_nonnull(resp->content_blocks);
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.id, "call_123");
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.name, "get_weather");
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.arguments, "{\"location\":\"San Francisco\"}");
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_TOOL_USE);
}
END_TEST

START_TEST(test_parse_chat_text_and_tool_calls)
{
    // Test response with both text and tool calls
    const char *json =
        "{"
        "  \"model\": \"gpt-4\","
        "  \"choices\": ["
        "    {"
        "      \"message\": {"
        "        \"role\": \"assistant\","
        "        \"content\": \"Let me check the weather for you.\","
        "        \"tool_calls\": ["
        "          {"
        "            \"id\": \"call_456\","
        "            \"function\": {"
        "              \"name\": \"get_weather\","
        "              \"arguments\": \"{\\\"location\\\":\\\"NYC\\\"}\""
        "            }"
        "          }"
        "        ]"
        "      },"
        "      \"finish_reason\": \"tool_calls\""
        "    }"
        "  ]"
        "}";

    ik_response_t *resp = NULL;
    res_t r = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_ok(&r));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 2);
    ck_assert_ptr_nonnull(resp->content_blocks);

    // First block should be text
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(resp->content_blocks[0].data.text.text, "Let me check the weather for you.");

    // Second block should be tool call
    ck_assert_int_eq(resp->content_blocks[1].type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(resp->content_blocks[1].data.tool_call.id, "call_456");
}
END_TEST

START_TEST(test_parse_chat_tool_call_missing_id)
{
    // Test tool call with missing id field (line 32-33)
    const char *json =
        "{"
        "  \"model\": \"gpt-4\","
        "  \"choices\": ["
        "    {"
        "      \"message\": {"
        "        \"role\": \"assistant\","
        "        \"tool_calls\": ["
        "          {"
        "            \"function\": {"
        "              \"name\": \"test\","
        "              \"arguments\": \"{}\""
        "            }"
        "          }"
        "        ]"
        "      },"
        "      \"finish_reason\": \"tool_calls\""
        "    }"
        "  ]"
        "}";

    ik_response_t *resp = NULL;
    res_t r = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_PARSE);
    ck_assert(strstr(r.err->msg, "missing 'id'") != NULL);
}
END_TEST

START_TEST(test_parse_chat_tool_call_id_not_string)
{
    // Test tool call with id not a string (line 36-37)
    const char *json =
        "{"
        "  \"model\": \"gpt-4\","
        "  \"choices\": ["
        "    {"
        "      \"message\": {"
        "        \"role\": \"assistant\","
        "        \"tool_calls\": ["
        "          {"
        "            \"id\": 123,"
        "            \"function\": {"
        "              \"name\": \"test\","
        "              \"arguments\": \"{}\""
        "            }"
        "          }"
        "        ]"
        "      },"
        "      \"finish_reason\": \"tool_calls\""
        "    }"
        "  ]"
        "}";

    ik_response_t *resp = NULL;
    res_t r = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_PARSE);
    ck_assert(strstr(r.err->msg, "'id' is not a string") != NULL);
}
END_TEST

START_TEST(test_parse_chat_tool_call_missing_function)
{
    // Test tool call with missing function field (line 44-45)
    const char *json =
        "{"
        "  \"model\": \"gpt-4\","
        "  \"choices\": ["
        "    {"
        "      \"message\": {"
        "        \"role\": \"assistant\","
        "        \"tool_calls\": ["
        "          {"
        "            \"id\": \"call_123\""
        "          }"
        "        ]"
        "      },"
        "      \"finish_reason\": \"tool_calls\""
        "    }"
        "  ]"
        "}";

    ik_response_t *resp = NULL;
    res_t r = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_PARSE);
    ck_assert(strstr(r.err->msg, "missing 'function'") != NULL);
}
END_TEST

START_TEST(test_parse_chat_tool_call_missing_name)
{
    // Test tool call with missing name field (line 50-51)
    const char *json =
        "{"
        "  \"model\": \"gpt-4\","
        "  \"choices\": ["
        "    {"
        "      \"message\": {"
        "        \"role\": \"assistant\","
        "        \"tool_calls\": ["
        "          {"
        "            \"id\": \"call_123\","
        "            \"function\": {"
        "              \"arguments\": \"{}\""
        "            }"
        "          }"
        "        ]"
        "      },"
        "      \"finish_reason\": \"tool_calls\""
        "    }"
        "  ]"
        "}";

    ik_response_t *resp = NULL;
    res_t r = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_PARSE);
    ck_assert(strstr(r.err->msg, "missing 'name'") != NULL);
}
END_TEST

START_TEST(test_parse_chat_tool_call_name_not_string)
{
    // Test tool call with name not a string (line 54-55)
    const char *json =
        "{"
        "  \"model\": \"gpt-4\","
        "  \"choices\": ["
        "    {"
        "      \"message\": {"
        "        \"role\": \"assistant\","
        "        \"tool_calls\": ["
        "          {"
        "            \"id\": \"call_123\","
        "            \"function\": {"
        "              \"name\": 456,"
        "              \"arguments\": \"{}\""
        "            }"
        "          }"
        "        ]"
        "      },"
        "      \"finish_reason\": \"tool_calls\""
        "    }"
        "  ]"
        "}";

    ik_response_t *resp = NULL;
    res_t r = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_PARSE);
    ck_assert(strstr(r.err->msg, "'name' is not a string") != NULL);
}
END_TEST

START_TEST(test_parse_chat_tool_call_missing_arguments)
{
    // Test tool call with missing arguments field (line 62-63)
    const char *json =
        "{"
        "  \"model\": \"gpt-4\","
        "  \"choices\": ["
        "    {"
        "      \"message\": {"
        "        \"role\": \"assistant\","
        "        \"tool_calls\": ["
        "          {"
        "            \"id\": \"call_123\","
        "            \"function\": {"
        "              \"name\": \"test\""
        "            }"
        "          }"
        "        ]"
        "      },"
        "      \"finish_reason\": \"tool_calls\""
        "    }"
        "  ]"
        "}";

    ik_response_t *resp = NULL;
    res_t r = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_PARSE);
    ck_assert(strstr(r.err->msg, "missing 'arguments'") != NULL);
}
END_TEST

START_TEST(test_parse_chat_tool_call_arguments_not_string)
{
    // Test tool call with arguments not a string (line 66-67)
    const char *json =
        "{"
        "  \"model\": \"gpt-4\","
        "  \"choices\": ["
        "    {"
        "      \"message\": {"
        "        \"role\": \"assistant\","
        "        \"tool_calls\": ["
        "          {"
        "            \"id\": \"call_123\","
        "            \"function\": {"
        "              \"name\": \"test\","
        "              \"arguments\": 789"
        "            }"
        "          }"
        "        ]"
        "      },"
        "      \"finish_reason\": \"tool_calls\""
        "    }"
        "  ]"
        "}";

    ik_response_t *resp = NULL;
    res_t r = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_PARSE);
    ck_assert(strstr(r.err->msg, "'arguments' is not a string") != NULL);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *openai_response_chat_suite(void)
{
    Suite *s = suite_create("OpenAI Response Chat");

    TCase *tc_finish_reason = tcase_create("Map Finish Reason");
    tcase_set_timeout(tc_finish_reason, 30);
    tcase_add_unchecked_fixture(tc_finish_reason, setup, teardown);
    tcase_add_test(tc_finish_reason, test_map_finish_reason_null);
    tcase_add_test(tc_finish_reason, test_map_finish_reason_stop);
    tcase_add_test(tc_finish_reason, test_map_finish_reason_length);
    tcase_add_test(tc_finish_reason, test_map_finish_reason_tool_calls);
    tcase_add_test(tc_finish_reason, test_map_finish_reason_content_filter);
    tcase_add_test(tc_finish_reason, test_map_finish_reason_error);
    tcase_add_test(tc_finish_reason, test_map_finish_reason_unknown);
    suite_add_tcase(s, tc_finish_reason);

    TCase *tc_parse_basic = tcase_create("Parse Chat Response - Basic");
    tcase_set_timeout(tc_parse_basic, 30);
    tcase_add_unchecked_fixture(tc_parse_basic, setup, teardown);
    tcase_add_test(tc_parse_basic, test_parse_chat_invalid_json);
    tcase_add_test(tc_parse_basic, test_parse_chat_not_object);
    tcase_add_test(tc_parse_basic, test_parse_chat_error_response);
    tcase_add_test(tc_parse_basic, test_parse_chat_error_response_no_message);
    tcase_add_test(tc_parse_basic, test_parse_chat_no_choices);
    tcase_add_test(tc_parse_basic, test_parse_chat_empty_choices);
    tcase_add_test(tc_parse_basic, test_parse_chat_no_message);
    tcase_add_test(tc_parse_basic, test_parse_chat_empty_content);
    tcase_add_test(tc_parse_basic, test_parse_chat_null_content);
    tcase_add_test(tc_parse_basic, test_parse_chat_text_content);
    tcase_add_test(tc_parse_basic, test_parse_chat_usage_with_reasoning_tokens);
    suite_add_tcase(s, tc_parse_basic);

    TCase *tc_tool_calls = tcase_create("Parse Chat Response - Tool Calls");
    tcase_set_timeout(tc_tool_calls, 30);
    tcase_add_unchecked_fixture(tc_tool_calls, setup, teardown);
    tcase_add_test(tc_tool_calls, test_parse_chat_tool_calls);
    tcase_add_test(tc_tool_calls, test_parse_chat_text_and_tool_calls);
    tcase_add_test(tc_tool_calls, test_parse_chat_tool_call_missing_id);
    tcase_add_test(tc_tool_calls, test_parse_chat_tool_call_id_not_string);
    tcase_add_test(tc_tool_calls, test_parse_chat_tool_call_missing_function);
    tcase_add_test(tc_tool_calls, test_parse_chat_tool_call_missing_name);
    tcase_add_test(tc_tool_calls, test_parse_chat_tool_call_name_not_string);
    tcase_add_test(tc_tool_calls, test_parse_chat_tool_call_missing_arguments);
    tcase_add_test(tc_tool_calls, test_parse_chat_tool_call_arguments_not_string);
    suite_add_tcase(s, tc_tool_calls);

    return s;
}

int main(void)
{
    Suite *s = openai_response_chat_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
