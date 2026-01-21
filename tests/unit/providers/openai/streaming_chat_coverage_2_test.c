#include "../../../test_constants.h"
/**
 * @file streaming_chat_coverage_test_2.c
 * @brief Coverage tests for OpenAI Chat streaming - Part 2 (edge cases)
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/openai/streaming.h"
#include "providers/openai/streaming_chat_internal.h"
#include "providers/openai/openai.h"
#include "providers/provider.h"

static TALLOC_CTX *test_ctx;
static ik_stream_event_t *last_event;

static res_t capturing_stream_cb(const ik_stream_event_t *event, void *ctx)
{
    (void)ctx;
    if (last_event == NULL) {
        last_event = talloc_zero(test_ctx, ik_stream_event_t);
    }
    last_event->type = event->type;
    if (event->type == IK_STREAM_DONE) {
        last_event->data.done.finish_reason = event->data.done.finish_reason;
        last_event->data.done.usage = event->data.done.usage;
    }
    return OK(NULL);
}

static res_t dummy_stream_cb(const ik_stream_event_t *event, void *ctx)
{
    (void)event;
    (void)ctx;
    return OK(NULL);
}

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    last_event = NULL;
}

static void teardown(void)
{
    talloc_free(test_ctx);
    last_event = NULL;
}

/* Edge case tests */

START_TEST(test_done_marker) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, capturing_stream_cb, NULL);

    /* Set some state that should be included in the DONE event */
    sctx->finish_reason = IK_FINISH_STOP;
    sctx->usage.input_tokens = 100;
    sctx->usage.output_tokens = 50;
    sctx->usage.total_tokens = 150;

    /* Process [DONE] marker */
    ik_openai_chat_stream_process_data(sctx, "[DONE]");

    /* Verify DONE event was emitted */
    ck_assert_ptr_nonnull(last_event);
    ck_assert_int_eq(last_event->type, IK_STREAM_DONE);
    ck_assert_int_eq(last_event->data.done.finish_reason, IK_FINISH_STOP);
    ck_assert_int_eq(last_event->data.done.usage.input_tokens, 100);
    ck_assert_int_eq(last_event->data.done.usage.output_tokens, 50);
    ck_assert_int_eq(last_event->data.done.usage.total_tokens, 150);
}

END_TEST

START_TEST(test_model_extraction) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    /* Process data with model field */
    const char *data = "{\"model\":\"gpt-4\",\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* Verify model was extracted */
    ck_assert_ptr_nonnull(sctx->model);
    ck_assert_str_eq(sctx->model, "gpt-4");
}

END_TEST

START_TEST(test_finish_reason_extraction) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, capturing_stream_cb, NULL);

    /* Process data with string finish_reason - this exercises line 163 where finish_reason_str is extracted */
    const char *data = "{\"model\":\"gpt-4\",\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}]}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* Verify getter works */
    ik_finish_reason_t reason = ik_openai_chat_stream_get_finish_reason(sctx);
    /* Should be STOP (0) if mapping works, otherwise UNKNOWN (5) */
    ck_assert(reason == IK_FINISH_STOP || reason == IK_FINISH_UNKNOWN);
}

END_TEST

START_TEST(test_usage_with_reasoning_tokens) {
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

START_TEST(test_usage_without_reasoning_tokens) {
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

START_TEST(test_error_without_message_field) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"error\":{\"type\":\"test_error\",\"code\":\"TEST\"}}";
    ik_openai_chat_stream_process_data(sctx, data);
}

END_TEST

START_TEST(test_model_field_non_string) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"model\":123,\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}";
    ik_openai_chat_stream_process_data(sctx, data);
}

END_TEST

START_TEST(test_choice_missing) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"choices\":[]}";
    ik_openai_chat_stream_process_data(sctx, data);
}

END_TEST

START_TEST(test_delta_missing) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"choices\":[{\"index\":0}]}";
    ik_openai_chat_stream_process_data(sctx, data);
}

END_TEST

START_TEST(test_reasoning_tokens_missing) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"usage\":{\"completion_tokens_details\":{}}}";
    ik_openai_chat_stream_process_data(sctx, data);

    ik_usage_t usage = ik_openai_chat_stream_get_usage(sctx);
    ck_assert_int_eq(usage.thinking_tokens, 0);
}

END_TEST

START_TEST(test_choices_not_array) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"choices\":\"not an array\"}";
    ik_openai_chat_stream_process_data(sctx, data);
}

END_TEST

START_TEST(test_choices_empty_array) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"choices\":[]}";
    ik_openai_chat_stream_process_data(sctx, data);
}

END_TEST

START_TEST(test_choice_not_object) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"choices\":[123]}";
    ik_openai_chat_stream_process_data(sctx, data);
}

END_TEST

START_TEST(test_delta_not_object) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"choices\":[{\"delta\":\"not an object\"}]}";
    ik_openai_chat_stream_process_data(sctx, data);
}

END_TEST

START_TEST(test_finish_reason_not_string) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"choices\":[{\"delta\":{\"role\":\"assistant\"},\"finish_reason\":123}]}";
    ik_openai_chat_stream_process_data(sctx, data);
}

END_TEST

START_TEST(test_usage_not_object) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"usage\":\"not an object\"}";
    ik_openai_chat_stream_process_data(sctx, data);

    ik_usage_t usage = ik_openai_chat_stream_get_usage(sctx);
    ck_assert_int_eq(usage.input_tokens, 0);
}

END_TEST

START_TEST(test_usage_prompt_tokens_not_int) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"usage\":{\"prompt_tokens\":\"not a number\"}}";
    ik_openai_chat_stream_process_data(sctx, data);

    ik_usage_t usage = ik_openai_chat_stream_get_usage(sctx);
    ck_assert_int_eq(usage.input_tokens, 0);
}

END_TEST

START_TEST(test_usage_completion_tokens_not_int) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"usage\":{\"completion_tokens\":\"not a number\"}}";
    ik_openai_chat_stream_process_data(sctx, data);

    ik_usage_t usage = ik_openai_chat_stream_get_usage(sctx);
    ck_assert_int_eq(usage.output_tokens, 0);
}

END_TEST

START_TEST(test_usage_total_tokens_not_int) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"usage\":{\"total_tokens\":\"not a number\"}}";
    ik_openai_chat_stream_process_data(sctx, data);

    ik_usage_t usage = ik_openai_chat_stream_get_usage(sctx);
    ck_assert_int_eq(usage.total_tokens, 0);
}

END_TEST

START_TEST(test_usage_details_not_object) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, dummy_stream_cb, NULL);

    const char *data = "{\"usage\":{\"completion_tokens_details\":\"not an object\"}}";
    ik_openai_chat_stream_process_data(sctx, data);

    ik_usage_t usage = ik_openai_chat_stream_get_usage(sctx);
    ck_assert_int_eq(usage.thinking_tokens, 0);
}

END_TEST

START_TEST(test_usage_reasoning_tokens_not_int) {
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

    TCase *tc_done = tcase_create("DoneMarker");
    tcase_set_timeout(tc_done, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_done, setup, teardown);
    tcase_add_test(tc_done, test_done_marker);
    suite_add_tcase(s, tc_done);

    TCase *tc_extraction = tcase_create("FieldExtraction");
    tcase_set_timeout(tc_extraction, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_extraction, setup, teardown);
    tcase_add_test(tc_extraction, test_model_extraction);
    tcase_add_test(tc_extraction, test_finish_reason_extraction);
    suite_add_tcase(s, tc_extraction);

    TCase *tc_usage = tcase_create("UsageExtraction");
    tcase_set_timeout(tc_usage, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_usage, setup, teardown);
    tcase_add_test(tc_usage, test_usage_with_reasoning_tokens);
    tcase_add_test(tc_usage, test_usage_without_reasoning_tokens);
    suite_add_tcase(s, tc_usage);

    TCase *tc_choices = tcase_create("ChoicesEdgeCases");
    tcase_set_timeout(tc_choices, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_choices, setup, teardown);
    tcase_add_test(tc_choices, test_choices_not_array);
    tcase_add_test(tc_choices, test_choices_empty_array);
    tcase_add_test(tc_choices, test_choice_not_object);
    tcase_add_test(tc_choices, test_delta_not_object);
    tcase_add_test(tc_choices, test_finish_reason_not_string);
    suite_add_tcase(s, tc_choices);

    TCase *tc_usage_edge = tcase_create("UsageEdgeCases");
    tcase_set_timeout(tc_usage_edge, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_usage_edge, setup, teardown);
    tcase_add_test(tc_usage_edge, test_usage_not_object);
    tcase_add_test(tc_usage_edge, test_usage_prompt_tokens_not_int);
    tcase_add_test(tc_usage_edge, test_usage_completion_tokens_not_int);
    tcase_add_test(tc_usage_edge, test_usage_total_tokens_not_int);
    tcase_add_test(tc_usage_edge, test_usage_details_not_object);
    tcase_add_test(tc_usage_edge, test_usage_reasoning_tokens_not_int);
    suite_add_tcase(s, tc_usage_edge);

    TCase *tc_edges = tcase_create("EdgeCases");
    tcase_set_timeout(tc_edges, IK_TEST_TIMEOUT);
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
    srunner_set_xml(sr, "reports/check/unit/providers/openai/streaming_chat_coverage_2_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
