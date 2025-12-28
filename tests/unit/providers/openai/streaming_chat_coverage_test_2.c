/**
 * @file streaming_chat_coverage_test_2.c
 * @brief Coverage tests for OpenAI Chat streaming - Part 2 (edge cases)
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/openai/streaming.h"
#include "providers/openai/openai.h"
#include "providers/provider.h"

static TALLOC_CTX *test_ctx;

static res_t dummy_stream_cb(const ik_stream_event_t *event, void *ctx)
{
    (void)event;
    (void)ctx;
    return OK(NULL);
}

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* Edge case tests */

START_TEST(test_usage_with_reasoning_tokens)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{"
        "\"choices\":[{\"delta\":{\"role\":\"assistant\"}}],"
        "\"usage\":{"
            "\"prompt_tokens\":10,"
            "\"completion_tokens\":20,"
            "\"total_tokens\":30,"
            "\"completion_tokens_details\":{\"reasoning_tokens\":5}"
        "}"
    "}";
    ik_openai_chat_stream_process_data(sctx, data);

    ik_usage_t usage = ik_openai_chat_stream_get_usage(sctx);
    ck_assert_int_eq(usage.input_tokens, 10);
    ck_assert_int_eq(usage.output_tokens, 20);
    ck_assert_int_eq(usage.total_tokens, 30);
    ck_assert_int_eq(usage.thinking_tokens, 5);
}

END_TEST

START_TEST(test_usage_without_reasoning_tokens)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{"
        "\"choices\":[{\"delta\":{\"role\":\"assistant\"}}],"
        "\"usage\":{"
            "\"prompt_tokens\":10,"
            "\"completion_tokens\":20,"
            "\"total_tokens\":30"
        "}"
    "}";
    ik_openai_chat_stream_process_data(sctx, data);

    ik_usage_t usage = ik_openai_chat_stream_get_usage(sctx);
    ck_assert_int_eq(usage.input_tokens, 10);
    ck_assert_int_eq(usage.output_tokens, 20);
    ck_assert_int_eq(usage.total_tokens, 30);
    ck_assert_int_eq(usage.thinking_tokens, 0);
}

END_TEST

START_TEST(test_error_without_message_field)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"error\":{\"type\":\"test_error\",\"code\":\"TEST\"}}";
    ik_openai_chat_stream_process_data(sctx, data);
}

END_TEST

START_TEST(test_model_field_non_string)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"model\":123,\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}";
    ik_openai_chat_stream_process_data(sctx, data);
}

END_TEST

START_TEST(test_choice_missing)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"choices\":[]}";
    ik_openai_chat_stream_process_data(sctx, data);
}

END_TEST

START_TEST(test_delta_missing)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"choices\":[{\"index\":0}]}";
    ik_openai_chat_stream_process_data(sctx, data);
}

END_TEST

START_TEST(test_reasoning_tokens_missing)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"usage\":{\"completion_tokens_details\":{}}}";
    ik_openai_chat_stream_process_data(sctx, data);

    ik_usage_t usage = ik_openai_chat_stream_get_usage(sctx);
    ck_assert_int_eq(usage.thinking_tokens, 0);
}

END_TEST

START_TEST(test_choices_not_array)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"choices\":\"not an array\"}";
    ik_openai_chat_stream_process_data(sctx, data);
}

END_TEST

START_TEST(test_choices_empty_array)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"choices\":[]}";
    ik_openai_chat_stream_process_data(sctx, data);
}

END_TEST

START_TEST(test_choice_not_object)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"choices\":[123]}";
    ik_openai_chat_stream_process_data(sctx, data);
}

END_TEST

START_TEST(test_delta_not_object)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"choices\":[{\"delta\":\"not an object\"}]}";
    ik_openai_chat_stream_process_data(sctx, data);
}

END_TEST

START_TEST(test_finish_reason_not_string)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"choices\":[{\"delta\":{\"role\":\"assistant\"},\"finish_reason\":123}]}";
    ik_openai_chat_stream_process_data(sctx, data);
}

END_TEST

START_TEST(test_usage_not_object)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"usage\":\"not an object\"}";
    ik_openai_chat_stream_process_data(sctx, data);

    ik_usage_t usage = ik_openai_chat_stream_get_usage(sctx);
    ck_assert_int_eq(usage.input_tokens, 0);
}

END_TEST

START_TEST(test_usage_prompt_tokens_not_int)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"usage\":{\"prompt_tokens\":\"not a number\"}}";
    ik_openai_chat_stream_process_data(sctx, data);

    ik_usage_t usage = ik_openai_chat_stream_get_usage(sctx);
    ck_assert_int_eq(usage.input_tokens, 0);
}

END_TEST

START_TEST(test_usage_completion_tokens_not_int)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"usage\":{\"completion_tokens\":\"not a number\"}}";
    ik_openai_chat_stream_process_data(sctx, data);

    ik_usage_t usage = ik_openai_chat_stream_get_usage(sctx);
    ck_assert_int_eq(usage.output_tokens, 0);
}

END_TEST

START_TEST(test_usage_total_tokens_not_int)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"usage\":{\"total_tokens\":\"not a number\"}}";
    ik_openai_chat_stream_process_data(sctx, data);

    ik_usage_t usage = ik_openai_chat_stream_get_usage(sctx);
    ck_assert_int_eq(usage.total_tokens, 0);
}

END_TEST

START_TEST(test_usage_details_not_object)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"usage\":{\"completion_tokens_details\":\"not an object\"}}";
    ik_openai_chat_stream_process_data(sctx, data);

    ik_usage_t usage = ik_openai_chat_stream_get_usage(sctx);
    ck_assert_int_eq(usage.thinking_tokens, 0);
}

END_TEST

START_TEST(test_usage_reasoning_tokens_not_int)
{
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"usage\":{\"completion_tokens_details\":{\"reasoning_tokens\":\"not a number\"}}}";
    ik_openai_chat_stream_process_data(sctx, data);

    ik_usage_t usage = ik_openai_chat_stream_get_usage(sctx);
    ck_assert_int_eq(usage.thinking_tokens, 0);
}

END_TEST

static Suite *streaming_chat_coverage_suite_2(void)
{
    Suite *s = suite_create("OpenAI Streaming Chat Coverage 2");

    TCase *tc_usage = tcase_create("UsageExtraction");
    tcase_add_checked_fixture(tc_usage, setup, teardown);
    tcase_add_test(tc_usage, test_usage_with_reasoning_tokens);
    tcase_add_test(tc_usage, test_usage_without_reasoning_tokens);
    suite_add_tcase(s, tc_usage);

    TCase *tc_choices = tcase_create("ChoicesEdgeCases");
    tcase_add_checked_fixture(tc_choices, setup, teardown);
    tcase_add_test(tc_choices, test_choices_not_array);
    tcase_add_test(tc_choices, test_choices_empty_array);
    tcase_add_test(tc_choices, test_choice_not_object);
    tcase_add_test(tc_choices, test_delta_not_object);
    tcase_add_test(tc_choices, test_finish_reason_not_string);
    suite_add_tcase(s, tc_choices);

    TCase *tc_usage_edge = tcase_create("UsageEdgeCases");
    tcase_add_checked_fixture(tc_usage_edge, setup, teardown);
    tcase_add_test(tc_usage_edge, test_usage_not_object);
    tcase_add_test(tc_usage_edge, test_usage_prompt_tokens_not_int);
    tcase_add_test(tc_usage_edge, test_usage_completion_tokens_not_int);
    tcase_add_test(tc_usage_edge, test_usage_total_tokens_not_int);
    tcase_add_test(tc_usage_edge, test_usage_details_not_object);
    tcase_add_test(tc_usage_edge, test_usage_reasoning_tokens_not_int);
    suite_add_tcase(s, tc_usage_edge);

    TCase *tc_edges = tcase_create("EdgeCases");
    tcase_add_checked_fixture(tc_edges, setup, teardown);
    tcase_add_test(tc_edges, test_error_without_message_field);
    tcase_add_test(tc_edges, test_model_field_non_string);
    tcase_add_test(tc_edges, test_choice_missing);
    tcase_add_test(tc_edges, test_delta_missing);
    tcase_add_test(tc_edges, test_reasoning_tokens_missing);
    suite_add_tcase(s, tc_edges);

    return s;
}

int main(void)
{
    Suite *s = streaming_chat_coverage_suite_2();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
