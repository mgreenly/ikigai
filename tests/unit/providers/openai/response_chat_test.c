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

START_TEST(test_map_finish_reason_stop)
{
    ik_finish_reason_t reason = ik_openai_map_chat_finish_reason("stop");
    ck_assert_int_eq(reason, IK_FINISH_STOP);
}
END_TEST

START_TEST(test_map_finish_reason_length)
{
    ik_finish_reason_t reason = ik_openai_map_chat_finish_reason("length");
    ck_assert_int_eq(reason, IK_FINISH_LENGTH);
}
END_TEST

START_TEST(test_map_finish_reason_tool_calls)
{
    ik_finish_reason_t reason = ik_openai_map_chat_finish_reason("tool_calls");
    ck_assert_int_eq(reason, IK_FINISH_TOOL_USE);
}
END_TEST

START_TEST(test_map_finish_reason_content_filter)
{
    ik_finish_reason_t reason = ik_openai_map_chat_finish_reason("content_filter");
    ck_assert_int_eq(reason, IK_FINISH_CONTENT_FILTER);
}
END_TEST

START_TEST(test_map_finish_reason_error)
{
    ik_finish_reason_t reason = ik_openai_map_chat_finish_reason("error");
    ck_assert_int_eq(reason, IK_FINISH_ERROR);
}
END_TEST

START_TEST(test_map_finish_reason_null)
{
    ik_finish_reason_t reason = ik_openai_map_chat_finish_reason(NULL);
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}
END_TEST

START_TEST(test_map_finish_reason_unknown)
{
    ik_finish_reason_t reason = ik_openai_map_chat_finish_reason("unknown_reason");
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}
END_TEST

/* ================================================================
 * Simple Response Parsing Tests
 * ================================================================ */

START_TEST(test_parse_simple_text_response)
{
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

START_TEST(test_parse_response_with_reasoning_tokens)
{
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

/* ================================================================
 * Tool Call Response Tests
 * ================================================================ */

START_TEST(test_parse_tool_call_response)
{
    const char *json = "{"
        "\"id\":\"chatcmpl-789\","
        "\"model\":\"gpt-4\","
        "\"choices\":[{"
            "\"index\":0,"
            "\"message\":{"
                "\"role\":\"assistant\","
                "\"content\":null,"
                "\"tool_calls\":[{"
                    "\"id\":\"call_abc123\","
                    "\"type\":\"function\","
                    "\"function\":{"
                        "\"name\":\"read_file\","
                        "\"arguments\":\"{\\\"path\\\":\\\"/etc/hosts\\\"}\""
                    "}"
                "}]"
            "},"
            "\"finish_reason\":\"tool_calls\""
        "}],"
        "\"usage\":{"
            "\"prompt_tokens\":100,"
            "\"completion_tokens\":20,"
            "\"total_tokens\":120"
        "}"
    "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_TOOL_USE);
    ck_assert_int_eq((int)resp->content_count, 1);
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.id, "call_abc123");
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.name, "read_file");
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.arguments,
                     "{\"path\":\"/etc/hosts\"}");
}
END_TEST

START_TEST(test_parse_multiple_tool_calls)
{
    const char *json = "{"
        "\"id\":\"chatcmpl-multi\","
        "\"model\":\"gpt-4\","
        "\"choices\":[{"
            "\"index\":0,"
            "\"message\":{"
                "\"role\":\"assistant\","
                "\"content\":null,"
                "\"tool_calls\":[{"
                    "\"id\":\"call_1\","
                    "\"type\":\"function\","
                    "\"function\":{"
                        "\"name\":\"read_file\","
                        "\"arguments\":\"{\\\"path\\\":\\\"/tmp/a\\\"}\""
                    "}"
                "},{"
                    "\"id\":\"call_2\","
                    "\"type\":\"function\","
                    "\"function\":{"
                        "\"name\":\"grep\","
                        "\"arguments\":\"{\\\"pattern\\\":\\\"test\\\",\\\"path\\\":\\\"/tmp/b\\\"}\""
                    "}"
                "}]"
            "},"
            "\"finish_reason\":\"tool_calls\""
        "}],"
        "\"usage\":{"
            "\"prompt_tokens\":50,"
            "\"completion_tokens\":30,"
            "\"total_tokens\":80"
        "}"
    "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_int_eq((int)resp->content_count, 2);

    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.id, "call_1");
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.name, "read_file");

    ck_assert_int_eq(resp->content_blocks[1].type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(resp->content_blocks[1].data.tool_call.id, "call_2");
    ck_assert_str_eq(resp->content_blocks[1].data.tool_call.name, "grep");
}
END_TEST

START_TEST(test_parse_text_with_tool_calls)
{
    /* Some models may include both content and tool_calls */
    const char *json = "{"
        "\"id\":\"chatcmpl-mixed\","
        "\"model\":\"gpt-4\","
        "\"choices\":[{"
            "\"index\":0,"
            "\"message\":{"
                "\"role\":\"assistant\","
                "\"content\":\"I'll read that file for you.\","
                "\"tool_calls\":[{"
                    "\"id\":\"call_xyz\","
                    "\"type\":\"function\","
                    "\"function\":{"
                        "\"name\":\"read_file\","
                        "\"arguments\":\"{\\\"path\\\":\\\"/tmp/test\\\"}\""
                    "}"
                "}]"
            "},"
            "\"finish_reason\":\"tool_calls\""
        "}],"
        "\"usage\":{"
            "\"prompt_tokens\":10,"
            "\"completion_tokens\":15,"
            "\"total_tokens\":25"
        "}"
    "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_int_eq((int)resp->content_count, 2);
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(resp->content_blocks[0].data.text.text, "I'll read that file for you.");
    ck_assert_int_eq(resp->content_blocks[1].type, IK_CONTENT_TOOL_CALL);
}
END_TEST

/* ================================================================
 * Empty and Edge Case Tests
 * ================================================================ */

START_TEST(test_parse_empty_choices)
{
    const char *json = "{"
        "\"id\":\"chatcmpl-empty\","
        "\"model\":\"gpt-4\","
        "\"choices\":[],"
        "\"usage\":{"
            "\"prompt_tokens\":0,"
            "\"completion_tokens\":0,"
            "\"total_tokens\":0"
        "}"
    "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_int_eq((int)resp->content_count, 0);
    ck_assert_ptr_null(resp->content_blocks);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_UNKNOWN);
}
END_TEST

START_TEST(test_parse_no_choices)
{
    const char *json = "{"
        "\"id\":\"chatcmpl-nochoices\","
        "\"model\":\"gpt-4\","
        "\"usage\":{"
            "\"prompt_tokens\":0,"
            "\"completion_tokens\":0,"
            "\"total_tokens\":0"
        "}"
    "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_int_eq((int)resp->content_count, 0);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_UNKNOWN);
}
END_TEST

START_TEST(test_parse_null_content)
{
    const char *json = "{"
        "\"id\":\"chatcmpl-null\","
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
    ck_assert_int_eq((int)resp->content_count, 0);
    ck_assert_ptr_null(resp->content_blocks);
}
END_TEST

START_TEST(test_parse_empty_string_content)
{
    const char *json = "{"
        "\"id\":\"chatcmpl-empty\","
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
    ck_assert_int_eq((int)resp->content_count, 0);
}
END_TEST

/* ================================================================
 * Error Response Tests
 * ================================================================ */

START_TEST(test_parse_error_response)
{
    const char *json = "{"
        "\"error\":{"
            "\"message\":\"Incorrect API key provided\","
            "\"type\":\"invalid_request_error\","
            "\"code\":\"invalid_api_key\""
        "}"
    "}";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_PROVIDER);
}
END_TEST

START_TEST(test_parse_malformed_json)
{
    const char *json = "{invalid json";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_PARSE);
}
END_TEST

START_TEST(test_parse_not_object)
{
    const char *json = "[\"array\", \"not\", \"object\"]";

    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_int_eq(result.err->code, ERR_PARSE);
}
END_TEST

/* ================================================================
 * Error Parsing Tests
 * ================================================================ */

START_TEST(test_parse_error_auth)
{
    const char *json = "{"
        "\"error\":{"
            "\"message\":\"Invalid API key\","
            "\"type\":\"invalid_request_error\","
            "\"code\":\"invalid_api_key\""
        "}"
    "}";

    ik_error_category_t category;
    char *message = NULL;
    res_t result = ik_openai_parse_error(test_ctx, 401, json, strlen(json),
                                          &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_AUTH);
    ck_assert_ptr_nonnull(message);
    ck_assert_str_eq(message, "invalid_request_error (invalid_api_key): Invalid API key");
}
END_TEST

START_TEST(test_parse_error_rate_limit)
{
    const char *json = "{"
        "\"error\":{"
            "\"message\":\"Rate limit exceeded\","
            "\"type\":\"rate_limit_error\""
        "}"
    "}";

    ik_error_category_t category;
    char *message = NULL;
    res_t result = ik_openai_parse_error(test_ctx, 429, json, strlen(json),
                                          &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_RATE_LIMIT);
    ck_assert_str_eq(message, "rate_limit_error: Rate limit exceeded");
}
END_TEST

START_TEST(test_parse_error_server)
{
    const char *json = "{"
        "\"error\":{"
            "\"message\":\"Internal server error\""
        "}"
    "}";

    ik_error_category_t category;
    char *message = NULL;
    res_t result = ik_openai_parse_error(test_ctx, 500, json, strlen(json),
                                          &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
    ck_assert_str_eq(message, "Internal server error");
}
END_TEST

START_TEST(test_parse_error_invalid_arg)
{
    ik_error_category_t category;
    char *message = NULL;
    res_t result = ik_openai_parse_error(test_ctx, 400, NULL, 0,
                                          &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_INVALID_ARG);
    ck_assert_str_eq(message, "HTTP 400");
}
END_TEST

START_TEST(test_parse_error_not_found)
{
    ik_error_category_t category;
    char *message = NULL;
    res_t result = ik_openai_parse_error(test_ctx, 404, NULL, 0,
                                          &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_NOT_FOUND);
    ck_assert_str_eq(message, "HTTP 404");
}
END_TEST

START_TEST(test_parse_error_unknown)
{
    ik_error_category_t category;
    char *message = NULL;
    res_t result = ik_openai_parse_error(test_ctx, 418, NULL, 0,
                                          &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_UNKNOWN);
    ck_assert_str_eq(message, "HTTP 418");
}
END_TEST

START_TEST(test_parse_error_malformed_json)
{
    const char *json = "{invalid";

    ik_error_category_t category;
    char *message = NULL;
    res_t result = ik_openai_parse_error(test_ctx, 500, json, strlen(json),
                                          &category, &message);

    ck_assert(!is_err(&result));
    ck_assert_int_eq(category, IK_ERR_CAT_SERVER);
    ck_assert_str_eq(message, "HTTP 500");
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *response_chat_suite(void)
{
    Suite *s = suite_create("OpenAI Chat Response Parsing");

    TCase *tc_finish_reason = tcase_create("Finish Reason Mapping");
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
    tcase_add_checked_fixture(tc_simple, setup, teardown);
    tcase_add_test(tc_simple, test_parse_simple_text_response);
    tcase_add_test(tc_simple, test_parse_response_with_reasoning_tokens);
    suite_add_tcase(s, tc_simple);

    TCase *tc_tools = tcase_create("Tool Call Responses");
    tcase_add_checked_fixture(tc_tools, setup, teardown);
    tcase_add_test(tc_tools, test_parse_tool_call_response);
    tcase_add_test(tc_tools, test_parse_multiple_tool_calls);
    tcase_add_test(tc_tools, test_parse_text_with_tool_calls);
    suite_add_tcase(s, tc_tools);

    TCase *tc_edge = tcase_create("Edge Cases");
    tcase_add_checked_fixture(tc_edge, setup, teardown);
    tcase_add_test(tc_edge, test_parse_empty_choices);
    tcase_add_test(tc_edge, test_parse_no_choices);
    tcase_add_test(tc_edge, test_parse_null_content);
    tcase_add_test(tc_edge, test_parse_empty_string_content);
    suite_add_tcase(s, tc_edge);

    TCase *tc_errors = tcase_create("Error Responses");
    tcase_add_checked_fixture(tc_errors, setup, teardown);
    tcase_add_test(tc_errors, test_parse_error_response);
    tcase_add_test(tc_errors, test_parse_malformed_json);
    tcase_add_test(tc_errors, test_parse_not_object);
    suite_add_tcase(s, tc_errors);

    TCase *tc_error_parse = tcase_create("Error Parsing");
    tcase_add_checked_fixture(tc_error_parse, setup, teardown);
    tcase_add_test(tc_error_parse, test_parse_error_auth);
    tcase_add_test(tc_error_parse, test_parse_error_rate_limit);
    tcase_add_test(tc_error_parse, test_parse_error_server);
    tcase_add_test(tc_error_parse, test_parse_error_invalid_arg);
    tcase_add_test(tc_error_parse, test_parse_error_not_found);
    tcase_add_test(tc_error_parse, test_parse_error_unknown);
    tcase_add_test(tc_error_parse, test_parse_error_malformed_json);
    suite_add_tcase(s, tc_error_parse);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s = response_chat_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
