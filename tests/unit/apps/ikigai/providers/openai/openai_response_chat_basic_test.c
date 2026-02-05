#include "tests/test_constants.h"
/**
 * @file openai_response_chat_basic_test.c
 * @brief Unit tests for OpenAI chat response parsing (basic cases)
 *
 * Tests ik_openai_map_chat_finish_reason and basic ik_openai_parse_chat_response cases
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/providers/openai/response.h"
#include "apps/ikigai/providers/provider.h"
#include "shared/error.h"

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

START_TEST(test_map_finish_reason_null) {
    ik_finish_reason_t result = ik_openai_map_chat_finish_reason(NULL);
    ck_assert_int_eq(result, IK_FINISH_UNKNOWN);
}
END_TEST

START_TEST(test_map_finish_reason_stop) {
    ik_finish_reason_t result = ik_openai_map_chat_finish_reason("stop");
    ck_assert_int_eq(result, IK_FINISH_STOP);
}
END_TEST

START_TEST(test_map_finish_reason_length) {
    ik_finish_reason_t result = ik_openai_map_chat_finish_reason("length");
    ck_assert_int_eq(result, IK_FINISH_LENGTH);
}
END_TEST

START_TEST(test_map_finish_reason_tool_calls) {
    ik_finish_reason_t result = ik_openai_map_chat_finish_reason("tool_calls");
    ck_assert_int_eq(result, IK_FINISH_TOOL_USE);
}
END_TEST

START_TEST(test_map_finish_reason_content_filter) {
    ik_finish_reason_t result = ik_openai_map_chat_finish_reason("content_filter");
    ck_assert_int_eq(result, IK_FINISH_CONTENT_FILTER);
}
END_TEST

START_TEST(test_map_finish_reason_error) {
    ik_finish_reason_t result = ik_openai_map_chat_finish_reason("error");
    ck_assert_int_eq(result, IK_FINISH_ERROR);
}
END_TEST

START_TEST(test_map_finish_reason_unknown) {
    ik_finish_reason_t result = ik_openai_map_chat_finish_reason("unknown_reason");
    ck_assert_int_eq(result, IK_FINISH_UNKNOWN);
}
END_TEST

/* ================================================================
 * ik_openai_parse_chat_response Basic Tests
 * ================================================================ */

START_TEST(test_parse_chat_invalid_json) {
    ik_response_t *resp = NULL;
    res_t r = ik_openai_parse_chat_response(test_ctx, "not valid json", 14, &resp);

    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_PARSE);
    ck_assert(strstr(r.err->msg, "Invalid JSON") != NULL);
}
END_TEST

START_TEST(test_parse_chat_not_object) {
    const char *json = "[1, 2, 3]";
    ik_response_t *resp = NULL;
    res_t r = ik_openai_parse_chat_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&r));
    ck_assert_int_eq(r.err->code, ERR_PARSE);
    ck_assert(strstr(r.err->msg, "not an object") != NULL);
}
END_TEST

START_TEST(test_parse_chat_error_response) {
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

START_TEST(test_parse_chat_error_response_no_message) {
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

START_TEST(test_parse_chat_no_choices) {
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

START_TEST(test_parse_chat_empty_choices) {
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

START_TEST(test_parse_chat_no_message) {
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

START_TEST(test_parse_chat_empty_content) {
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

START_TEST(test_parse_chat_null_content) {
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

START_TEST(test_parse_chat_text_content) {
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

START_TEST(test_parse_chat_usage_with_reasoning_tokens) {
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

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *openai_response_chat_basic_suite(void)
{
    Suite *s = suite_create("OpenAI Response Chat Basic");

    TCase *tc_finish_reason = tcase_create("Map Finish Reason");
    tcase_set_timeout(tc_finish_reason, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_finish_reason, setup, teardown);
    tcase_add_test(tc_finish_reason, test_map_finish_reason_null);
    tcase_add_test(tc_finish_reason, test_map_finish_reason_stop);
    tcase_add_test(tc_finish_reason, test_map_finish_reason_length);
    tcase_add_test(tc_finish_reason, test_map_finish_reason_tool_calls);
    tcase_add_test(tc_finish_reason, test_map_finish_reason_content_filter);
    tcase_add_test(tc_finish_reason, test_map_finish_reason_error);
    tcase_add_test(tc_finish_reason, test_map_finish_reason_unknown);
    suite_add_tcase(s, tc_finish_reason);

    TCase *tc_parse_basic = tcase_create("Parse Chat Response");
    tcase_set_timeout(tc_parse_basic, IK_TEST_TIMEOUT);
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

    return s;
}

int main(void)
{
    Suite *s = openai_response_chat_basic_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/openai/openai_response_chat_basic_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
