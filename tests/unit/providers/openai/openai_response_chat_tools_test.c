/**
 * @file openai_response_chat_tools_test.c
 * @brief Unit tests for OpenAI chat response tool call parsing
 *
 * Tests tool call parsing in ik_openai_parse_chat_response
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
 * Tool Call Parsing Tests
 * ================================================================ */

START_TEST(test_parse_chat_tool_calls)
{
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

static Suite *openai_response_chat_tools_suite(void)
{
    Suite *s = suite_create("OpenAI Response Chat Tools");

    TCase *tc_tool_calls = tcase_create("Tool Call Parsing");
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
    Suite *s = openai_response_chat_tools_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
