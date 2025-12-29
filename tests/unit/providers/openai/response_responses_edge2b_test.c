/**
 * @file response_responses_edge2b_test.c
 * @brief Tests for OpenAI Responses API edge cases - invalid types (part 2)
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
 * Edge Cases - Invalid Types (Part 2)
 * ================================================================ */

START_TEST(test_parse_response_refusal_no_field) {
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

END_TEST START_TEST(test_parse_response_refusal_not_string)
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

END_TEST START_TEST(test_parse_response_usage_non_int_values)
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

END_TEST START_TEST(test_parse_response_model_not_string)
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

END_TEST START_TEST(test_parse_response_status_not_string)
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

END_TEST START_TEST(test_parse_response_incomplete_reason_not_string)
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

END_TEST START_TEST(test_parse_response_function_call_call_id_not_string)
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
 * Test Suite
 * ================================================================ */

static Suite *response_responses_edge2b_suite(void)
{
    Suite *s = suite_create("OpenAI Responses API Edge Cases (Invalid Types Part 2)");

    TCase *tc_edge = tcase_create("Invalid Types Part 2");
    tcase_add_checked_fixture(tc_edge, setup, teardown);
    tcase_add_test(tc_edge, test_parse_response_refusal_no_field);
    tcase_add_test(tc_edge, test_parse_response_refusal_not_string);
    tcase_add_test(tc_edge, test_parse_response_usage_non_int_values);
    tcase_add_test(tc_edge, test_parse_response_model_not_string);
    tcase_add_test(tc_edge, test_parse_response_status_not_string);
    tcase_add_test(tc_edge, test_parse_response_incomplete_reason_not_string);
    tcase_add_test(tc_edge, test_parse_response_function_call_call_id_not_string);
    suite_add_tcase(s, tc_edge);

    return s;
}

int main(void)
{
    int32_t number_failed;
    Suite *s = response_responses_edge2b_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
