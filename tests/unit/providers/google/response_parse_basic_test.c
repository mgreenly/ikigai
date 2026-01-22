#include "../../../test_constants.h"
/**
 * @file response_parse_basic_test.c
 * @brief Unit tests for Google response parsing - basic cases
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/google/response.h"
#include "providers/provider.h"

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
 * Basic Response Parsing Tests
 * ================================================================ */

START_TEST(test_parse_simple_text_response) {
    const char *json = "{"
                       "\"modelVersion\":\"gemini-2.5-flash\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":[{\"text\":\"Hello world\"}]},"
                       "\"finishReason\":\"STOP\""
                       "}],"
                       "\"usageMetadata\":{"
                       "\"promptTokenCount\":10,"
                       "\"candidatesTokenCount\":5,"
                       "\"thoughtsTokenCount\":0,"
                       "\"totalTokenCount\":15"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_str_eq(resp->model, "gemini-2.5-flash");
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_STOP);
    ck_assert_uint_eq((unsigned int)resp->content_count, 1);
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(resp->content_blocks[0].data.text.text, "Hello world");
    ck_assert_int_eq(resp->usage.input_tokens, 10);
    ck_assert_int_eq(resp->usage.output_tokens, 5);
    ck_assert_int_eq(resp->usage.thinking_tokens, 0);
    ck_assert_int_eq(resp->usage.total_tokens, 15);
}
END_TEST

START_TEST(test_parse_thinking_response) {
    const char *json = "{"
                       "\"modelVersion\":\"gemini-3\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":["
                       "{\"text\":\"Let me think...\",\"thought\":true},"
                       "{\"text\":\"The answer is 42\"}"
                       "]},"
                       "\"finishReason\":\"STOP\""
                       "}],"
                       "\"usageMetadata\":{"
                       "\"promptTokenCount\":10,"
                       "\"candidatesTokenCount\":20,"
                       "\"thoughtsTokenCount\":8,"
                       "\"totalTokenCount\":30"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_uint_eq((unsigned int)resp->content_count, 2);

    // First block is thinking
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_THINKING);
    ck_assert_str_eq(resp->content_blocks[0].data.thinking.text, "Let me think...");

    // Second block is text
    ck_assert_int_eq(resp->content_blocks[1].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(resp->content_blocks[1].data.text.text, "The answer is 42");

    // Verify token calculation: output = candidates - thoughts = 20 - 8 = 12
    ck_assert_int_eq(resp->usage.thinking_tokens, 8);
    ck_assert_int_eq(resp->usage.output_tokens, 12);
}

END_TEST

START_TEST(test_parse_function_call_response) {
    const char *json = "{"
                       "\"modelVersion\":\"gemini-2.5-pro\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":[{"
                       "\"functionCall\":{"
                       "\"name\":\"get_weather\","
                       "\"args\":{\"city\":\"London\",\"units\":\"metric\"}"
                       "}"
                       "}]},"
                       "\"finishReason\":\"STOP\""
                       "}],"
                       "\"usageMetadata\":{"
                       "\"promptTokenCount\":15,"
                       "\"candidatesTokenCount\":10,"
                       "\"totalTokenCount\":25"
                       "}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_uint_eq((unsigned int)resp->content_count, 1);
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_TOOL_CALL);

    // Tool call has generated ID (22 chars)
    ck_assert_ptr_nonnull(resp->content_blocks[0].data.tool_call.id);
    ck_assert_uint_eq((unsigned int)strlen(resp->content_blocks[0].data.tool_call.id), 22);

    // Tool name and args
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.name, "get_weather");
    ck_assert_ptr_nonnull(strstr(resp->content_blocks[0].data.tool_call.arguments, "London"));
    ck_assert_ptr_nonnull(strstr(resp->content_blocks[0].data.tool_call.arguments, "metric"));
}

END_TEST

START_TEST(test_parse_empty_candidates) {
    const char *json = "{"
                       "\"modelVersion\":\"gemini-2.5-flash\","
                       "\"candidates\":[],"
                       "\"usageMetadata\":{\"totalTokenCount\":0}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_uint_eq((unsigned int)resp->content_count, 0);
    ck_assert_ptr_null(resp->content_blocks);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_UNKNOWN);
}

END_TEST

START_TEST(test_parse_no_candidates) {
    const char *json = "{"
                       "\"modelVersion\":\"gemini-2.5-flash\","
                       "\"usageMetadata\":{\"totalTokenCount\":5}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_uint_eq((unsigned int)resp->content_count, 0);
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_UNKNOWN);
}

END_TEST

START_TEST(test_parse_empty_parts_array) {
    const char *json = "{"
                       "\"modelVersion\":\"gemini-2.5-flash\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":[]},"
                       "\"finishReason\":\"STOP\""
                       "}],"
                       "\"usageMetadata\":{\"totalTokenCount\":10}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_uint_eq((unsigned int)resp->content_count, 0);
    ck_assert_ptr_null(resp->content_blocks);
}

END_TEST

START_TEST(test_parse_thought_flag_false) {
    const char *json = "{"
                       "\"modelVersion\":\"gemini-2.5-flash\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":["
                       "{\"text\":\"Normal text\",\"thought\":false}"
                       "]},"
                       "\"finishReason\":\"STOP\""
                       "}],"
                       "\"usageMetadata\":{\"totalTokenCount\":10}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_uint_eq((unsigned int)resp->content_count, 1);
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(resp->content_blocks[0].data.text.text, "Normal text");
}

END_TEST

START_TEST(test_parse_function_call_no_args) {
    const char *json = "{"
                       "\"modelVersion\":\"gemini-2.5-pro\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":[{"
                       "\"functionCall\":{"
                       "\"name\":\"list_files\""
                       "}"
                       "}]},"
                       "\"finishReason\":\"STOP\""
                       "}],"
                       "\"usageMetadata\":{\"totalTokenCount\":10}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_uint_eq((unsigned int)resp->content_count, 1);
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.name, "list_files");
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.arguments, "{}");
}

END_TEST

START_TEST(test_parse_thought_signature) {
    const char *json = "{"
                       "\"modelVersion\":\"gemini-3\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":[{\"text\":\"Hello\"}]},"
                       "\"finishReason\":\"STOP\""
                       "}],"
                       "\"thoughtSignature\":\"enc_sig_abc123\","
                       "\"usageMetadata\":{\"totalTokenCount\":10}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp->provider_data);
    ck_assert_ptr_nonnull(strstr(resp->provider_data, "thought_signature"));
    ck_assert_ptr_nonnull(strstr(resp->provider_data, "enc_sig_abc123"));
}

END_TEST

START_TEST(test_parse_no_thought_signature) {
    const char *json = "{"
                       "\"modelVersion\":\"gemini-2.5-flash\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":[{\"text\":\"Hello\"}]},"
                       "\"finishReason\":\"STOP\""
                       "}],"
                       "\"usageMetadata\":{\"totalTokenCount\":10}"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_null(resp->provider_data);
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_response_parse_basic_suite(void)
{
    Suite *s = suite_create("Google Response Parsing - Basic");

    TCase *tc_parse = tcase_create("Basic Parsing");
    tcase_set_timeout(tc_parse, IK_TEST_TIMEOUT);
    tcase_add_unchecked_fixture(tc_parse, setup, teardown);
    tcase_add_test(tc_parse, test_parse_simple_text_response);
    tcase_add_test(tc_parse, test_parse_thinking_response);
    tcase_add_test(tc_parse, test_parse_function_call_response);
    tcase_add_test(tc_parse, test_parse_empty_candidates);
    tcase_add_test(tc_parse, test_parse_no_candidates);
    tcase_add_test(tc_parse, test_parse_empty_parts_array);
    tcase_add_test(tc_parse, test_parse_thought_flag_false);
    tcase_add_test(tc_parse, test_parse_function_call_no_args);
    tcase_add_test(tc_parse, test_parse_thought_signature);
    tcase_add_test(tc_parse, test_parse_no_thought_signature);
    suite_add_tcase(s, tc_parse);

    return s;
}

int main(void)
{
    Suite *s = google_response_parse_basic_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/providers/google/response_parse_basic_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
