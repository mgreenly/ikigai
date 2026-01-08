/**
 * @file response_responses_null_test.c
 * @brief Tests for NULL field handling in OpenAI Responses API parsing
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
 * Tests for missing usage fields
 * ================================================================ */

START_TEST(test_usage_missing_tokens_fields) {
    // Missing prompt_tokens
    const char *json1 = "{\"model\":\"gpt-4o\",\"status\":\"completed\",\"output\":[],"
                        "\"usage\":{\"completion_tokens\":10,\"total_tokens\":15}}";
    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json1, strlen(json1), &resp);
    ck_assert(!is_err(&result));
    ck_assert_int_eq(resp->usage.input_tokens, 0);
    ck_assert_int_eq(resp->usage.output_tokens, 10);
    talloc_free(resp);

    // Missing completion_tokens
    const char *json2 = "{\"model\":\"gpt-4o\",\"status\":\"completed\",\"output\":[],"
                        "\"usage\":{\"prompt_tokens\":5,\"total_tokens\":15}}";
    result = ik_openai_parse_responses_response(test_ctx, json2, strlen(json2), &resp);
    ck_assert(!is_err(&result));
    ck_assert_int_eq(resp->usage.input_tokens, 5);
    ck_assert_int_eq(resp->usage.output_tokens, 0);
    talloc_free(resp);

    // Missing total_tokens
    const char *json3 = "{\"model\":\"gpt-4o\",\"status\":\"completed\",\"output\":[],"
                        "\"usage\":{\"prompt_tokens\":5,\"completion_tokens\":10}}";
    result = ik_openai_parse_responses_response(test_ctx, json3, strlen(json3), &resp);
    ck_assert(!is_err(&result));
    ck_assert_int_eq(resp->usage.total_tokens, 0);
}
END_TEST

START_TEST(test_usage_missing_reasoning_details) {
    // Missing completion_tokens_details
    const char *json1 = "{\"model\":\"gpt-4o\",\"status\":\"completed\",\"output\":[],"
                        "\"usage\":{\"prompt_tokens\":5,\"completion_tokens\":10,\"total_tokens\":15}}";
    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json1, strlen(json1), &resp);
    ck_assert(!is_err(&result));
    ck_assert_int_eq(resp->usage.thinking_tokens, 0);
    talloc_free(resp);

    // Empty completion_tokens_details
    const char *json2 = "{\"model\":\"gpt-4o\",\"status\":\"completed\",\"output\":[],"
                        "\"usage\":{\"prompt_tokens\":5,\"completion_tokens\":10,\"total_tokens\":15,"
                        "\"completion_tokens_details\":{}}}";
    result = ik_openai_parse_responses_response(test_ctx, json2, strlen(json2), &resp);
    ck_assert(!is_err(&result));
    ck_assert_int_eq(resp->usage.thinking_tokens, 0);
}
END_TEST

/* ================================================================
 * Tests for missing model field
 * ================================================================ */

START_TEST(test_response_model_variations) {
    // Missing model field
    const char *json1 = "{\"status\":\"completed\",\"output\":[]}";
    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json1, strlen(json1), &resp);
    ck_assert(!is_err(&result));
    ck_assert_ptr_null(resp->model);
    talloc_free(resp);

    // Null model
    const char *json2 = "{\"model\":null,\"status\":\"completed\",\"output\":[]}";
    result = ik_openai_parse_responses_response(test_ctx, json2, strlen(json2), &resp);
    ck_assert(!is_err(&result));
    ck_assert_ptr_null(resp->model);
}
END_TEST

START_TEST(test_response_missing_status) {
    const char *json = "{\"model\":\"gpt-4o\",\"output\":[]}";
    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json, strlen(json), &resp);
    ck_assert(!is_err(&result));
    ck_assert_int_eq(resp->finish_reason, IK_FINISH_UNKNOWN);
}
END_TEST

/* ================================================================
 * Tests for function call with call_id variations
 * ================================================================ */

START_TEST(test_function_call_id_variations) {
    // call_id null but id present
    const char *json1 = "{\"status\":\"completed\",\"output\":[{\"type\":\"function_call\","
                        "\"id\":\"func-123\",\"call_id\":null,\"name\":\"test\",\"arguments\":\"{}\"}]}";
    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json1, strlen(json1), &resp);
    ck_assert(!is_err(&result));
    ck_assert_str_eq(resp->content_blocks[0].data.tool_call.id, "func-123");
    talloc_free(resp);

    // Missing both id and call_id
    const char *json2 = "{\"status\":\"completed\",\"output\":[{\"type\":\"function_call\","
                        "\"name\":\"test\",\"arguments\":\"{}\"}]}";
    result = ik_openai_parse_responses_response(test_ctx, json2, strlen(json2), &resp);
    ck_assert(is_err(&result));
}
END_TEST

/* ================================================================
 * Tests for content with NULL values
 * ================================================================ */

START_TEST(test_message_content_invalid) {
    // Content not an array
    const char *json1 = "{\"status\":\"completed\",\"output\":[{\"type\":\"message\",\"content\":\"string\"}]}";
    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json1, strlen(json1), &resp);
    ck_assert(!is_err(&result));
    ck_assert_int_eq((int)resp->content_count, 0);
    talloc_free(resp);

    // Missing content
    const char *json2 = "{\"status\":\"completed\",\"output\":[{\"type\":\"message\"}]}";
    result = ik_openai_parse_responses_response(test_ctx, json2, strlen(json2), &resp);
    ck_assert(!is_err(&result));
    ck_assert_int_eq((int)resp->content_count, 0);
}
END_TEST

START_TEST(test_content_item_type_invalid) {
    // Missing type
    const char *json1 = "{\"status\":\"completed\",\"output\":[{\"type\":\"message\","
                        "\"content\":[{\"text\":\"Hello\"}]}]}";
    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json1, strlen(json1), &resp);
    ck_assert(!is_err(&result));
    ck_assert_int_eq((int)resp->content_count, 0);
    talloc_free(resp);

    // Type not a string
    const char *json2 = "{\"status\":\"completed\",\"output\":[{\"type\":\"message\","
                        "\"content\":[{\"type\":123,\"text\":\"Hi\"}]}]}";
    result = ik_openai_parse_responses_response(test_ctx, json2, strlen(json2), &resp);
    ck_assert(!is_err(&result));
    ck_assert_int_eq((int)resp->content_count, 0);
}
END_TEST

START_TEST(test_output_text_invalid_text) {
    // Missing text field
    const char *json1 = "{\"status\":\"completed\",\"output\":[{\"type\":\"message\","
                        "\"content\":[{\"type\":\"output_text\"}]}]}";
    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json1, strlen(json1), &resp);
    ck_assert(!is_err(&result));
    ck_assert_int_eq((int)resp->content_count, 0);
    talloc_free(resp);

    // Null text
    const char *json2 = "{\"status\":\"completed\",\"output\":[{\"type\":\"message\","
                        "\"content\":[{\"type\":\"output_text\",\"text\":null}]}]}";
    result = ik_openai_parse_responses_response(test_ctx, json2, strlen(json2), &resp);
    ck_assert(!is_err(&result));
    ck_assert_int_eq((int)resp->content_count, 0);
}
END_TEST

START_TEST(test_refusal_invalid_refusal) {
    // Missing refusal field
    const char *json1 = "{\"status\":\"completed\",\"output\":[{\"type\":\"message\","
                        "\"content\":[{\"type\":\"refusal\"}]}]}";
    ik_response_t *resp = NULL;
    res_t result = ik_openai_parse_responses_response(test_ctx, json1, strlen(json1), &resp);
    ck_assert(!is_err(&result));
    ck_assert_int_eq((int)resp->content_count, 0);
    talloc_free(resp);

    // Null refusal
    const char *json2 = "{\"status\":\"completed\",\"output\":[{\"type\":\"message\","
                        "\"content\":[{\"type\":\"refusal\",\"refusal\":null}]}]}";
    result = ik_openai_parse_responses_response(test_ctx, json2, strlen(json2), &resp);
    ck_assert(!is_err(&result));
    ck_assert_int_eq((int)resp->content_count, 0);
}
END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *response_responses_null_suite(void)
{
    Suite *s = suite_create("OpenAI Responses API NULL Handling Tests");

    TCase *tc = tcase_create("NULL and Missing Fields");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_usage_missing_tokens_fields);
    tcase_add_test(tc, test_usage_missing_reasoning_details);
    tcase_add_test(tc, test_response_model_variations);
    tcase_add_test(tc, test_response_missing_status);
    tcase_add_test(tc, test_function_call_id_variations);
    tcase_add_test(tc, test_message_content_invalid);
    tcase_add_test(tc, test_content_item_type_invalid);
    tcase_add_test(tc, test_output_text_invalid_text);
    tcase_add_test(tc, test_refusal_invalid_refusal);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = response_responses_null_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
