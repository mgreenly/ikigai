/**
 * @file response_parts_coverage_test.c
 * @brief Coverage tests for Google response.c parse_content_parts edge cases
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
 * parse_content_parts Edge Cases
 * ================================================================ */

START_TEST(test_parse_empty_parts_array)
{
    const char *json = "{"
                       "\"modelVersion\":\"gemini-2.5-flash\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":[]},"
                       "\"finishReason\":\"STOP\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_ptr_nonnull(resp);
    ck_assert_uint_eq((unsigned int)resp->content_count, 0);
    ck_assert_ptr_null(resp->content_blocks);
}
END_TEST

START_TEST(test_parse_function_call_missing_name)
{
    const char *json = "{"
                       "\"modelVersion\":\"gemini-2.5-pro\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":[{"
                       "\"functionCall\":{\"args\":{\"city\":\"London\"}}"
                       "}]},"
                       "\"finishReason\":\"STOP\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_ptr_nonnull(strstr(result.err->msg, "missing 'name'"));
    talloc_free(result.err);
}
END_TEST

START_TEST(test_parse_function_call_name_not_string)
{
    const char *json = "{"
                       "\"modelVersion\":\"gemini-2.5-pro\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":[{"
                       "\"functionCall\":{\"name\":123,\"args\":{}}"
                       "}]},"
                       "\"finishReason\":\"STOP\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_ptr_nonnull(strstr(result.err->msg, "'name' is not a string"));
    talloc_free(result.err);
}
END_TEST

START_TEST(test_parse_function_call_with_args)
{
    const char *json = "{"
                       "\"modelVersion\":\"gemini-2.5-pro\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":[{"
                       "\"functionCall\":{\"name\":\"get_weather\",\"args\":{\"city\":\"London\"}}"
                       "}]},"
                       "\"finishReason\":\"STOP\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_uint_eq((unsigned int)resp->content_count, 1);
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.name, "get_weather");
    ck_assert_ptr_nonnull(strstr(resp->content_blocks[0].data.tool_call.arguments, "London"));
}
END_TEST

START_TEST(test_parse_function_call_no_args)
{
    const char *json = "{"
                       "\"modelVersion\":\"gemini-2.5-pro\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":[{"
                       "\"functionCall\":{\"name\":\"get_time\"}"
                       "}]},"
                       "\"finishReason\":\"STOP\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_uint_eq((unsigned int)resp->content_count, 1);
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.name, "get_time");
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.arguments, "{}");
}
END_TEST

START_TEST(test_parse_part_with_thought_flag_true)
{
    const char *json = "{"
                       "\"modelVersion\":\"gemini-3\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":["
                       "{\"text\":\"Analyzing...\",\"thought\":true}"
                       "]},"
                       "\"finishReason\":\"STOP\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_uint_eq((unsigned int)resp->content_count, 1);
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_THINKING);
    ck_assert_str_eq(resp->content_blocks[0].data.thinking.text, "Analyzing...");
}
END_TEST

START_TEST(test_parse_part_with_thought_flag_false)
{
    const char *json = "{"
                       "\"modelVersion\":\"gemini-3\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":["
                       "{\"text\":\"Normal text\",\"thought\":false}"
                       "]},"
                       "\"finishReason\":\"STOP\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_uint_eq((unsigned int)resp->content_count, 1);
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(resp->content_blocks[0].data.text.text, "Normal text");
}
END_TEST

START_TEST(test_parse_part_without_text_or_function_call)
{
    const char *json = "{"
                       "\"modelVersion\":\"gemini-2.5-flash\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":["
                       "{\"other\":\"field\"}"
                       "]},"
                       "\"finishReason\":\"STOP\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_uint_eq((unsigned int)resp->content_count, 1);
}
END_TEST

START_TEST(test_parse_part_text_not_string)
{
    const char *json = "{"
                       "\"modelVersion\":\"gemini-2.5-flash\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":["
                       "{\"text\":42}"
                       "]},"
                       "\"finishReason\":\"STOP\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(is_err(&result));
    ck_assert_ptr_nonnull(strstr(result.err->msg, "'text' is not a string"));
    talloc_free(result.err);
}
END_TEST

START_TEST(test_parse_part_without_thought_flag)
{
    const char *json = "{"
                       "\"modelVersion\":\"gemini-3\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":["
                       "{\"text\":\"Normal text\"}"
                       "]},"
                       "\"finishReason\":\"STOP\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_uint_eq((unsigned int)resp->content_count, 1);
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(resp->content_blocks[0].data.text.text, "Normal text");
}
END_TEST

START_TEST(test_parse_part_with_thought_string)
{
    const char *json = "{"
                       "\"modelVersion\":\"gemini-3\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":["
                       "{\"text\":\"Text with string thought\",\"thought\":\"yes\"}"
                       "]},"
                       "\"finishReason\":\"STOP\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_uint_eq((unsigned int)resp->content_count, 1);
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(resp->content_blocks[0].data.text.text, "Text with string thought");
}
END_TEST

START_TEST(test_parse_multiple_parts_mixed)
{
    const char *json = "{"
                       "\"modelVersion\":\"gemini-3\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":["
                       "{\"text\":\"First part\"},"
                       "{\"text\":\"Second part\",\"thought\":true},"
                       "{\"functionCall\":{\"name\":\"test_tool\",\"args\":{\"x\":1}}},"
                       "{\"text\":\"Third part\"}"
                       "]},"
                       "\"finishReason\":\"STOP\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_uint_eq((unsigned int)resp->content_count, 4);
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(resp->content_blocks[0].data.text.text, "First part");
    ck_assert_int_eq(resp->content_blocks[1].type, IK_CONTENT_THINKING);
    ck_assert_str_eq(resp->content_blocks[1].data.thinking.text, "Second part");
    ck_assert_int_eq(resp->content_blocks[2].type, IK_CONTENT_TOOL_CALL);
    ck_assert_str_eq(resp->content_blocks[2].data.tool_call.name, "test_tool");
    ck_assert_int_eq(resp->content_blocks[3].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(resp->content_blocks[3].data.text.text, "Third part");
}
END_TEST

START_TEST(test_parse_part_with_thought_null_value)
{
    const char *json = "{"
                       "\"modelVersion\":\"gemini-3\","
                       "\"candidates\":[{"
                       "\"content\":{\"parts\":["
                       "{\"text\":\"Normal text\",\"thought\":null}"
                       "]},"
                       "\"finishReason\":\"STOP\""
                       "}]"
                       "}";

    ik_response_t *resp = NULL;
    res_t result = ik_google_parse_response(test_ctx, json, strlen(json), &resp);

    ck_assert(!is_err(&result));
    ck_assert_uint_eq((unsigned int)resp->content_count, 1);
    ck_assert_int_eq(resp->content_blocks[0].type, IK_CONTENT_TEXT);
    ck_assert_str_eq(resp->content_blocks[0].data.text.text, "Normal text");
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_response_parts_coverage_suite(void)
{
    Suite *s = suite_create("Google Response Parts Coverage");

    TCase *tc_parts = tcase_create("parse_content_parts edge cases");
    tcase_set_timeout(tc_parts, 30);
    tcase_add_unchecked_fixture(tc_parts, setup, teardown);
    tcase_add_test(tc_parts, test_parse_empty_parts_array);
    tcase_add_test(tc_parts, test_parse_function_call_missing_name);
    tcase_add_test(tc_parts, test_parse_function_call_name_not_string);
    tcase_add_test(tc_parts, test_parse_function_call_with_args);
    tcase_add_test(tc_parts, test_parse_function_call_no_args);
    tcase_add_test(tc_parts, test_parse_part_with_thought_flag_true);
    tcase_add_test(tc_parts, test_parse_part_with_thought_flag_false);
    tcase_add_test(tc_parts, test_parse_part_without_text_or_function_call);
    tcase_add_test(tc_parts, test_parse_part_text_not_string);
    tcase_add_test(tc_parts, test_parse_part_without_thought_flag);
    tcase_add_test(tc_parts, test_parse_part_with_thought_string);
    tcase_add_test(tc_parts, test_parse_multiple_parts_mixed);
    tcase_add_test(tc_parts, test_parse_part_with_thought_null_value);
    suite_add_tcase(s, tc_parts);

    return s;
}

int main(void)
{
    Suite *s = google_response_parts_coverage_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
