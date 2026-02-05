#include "tests/test_constants.h"
/**
 * @file openai_streaming_responses_events2_test.c
 * @brief Tests for OpenAI Responses API event processing edge cases (part 2)
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/providers/openai/streaming.h"
#include "apps/ikigai/providers/openai/streaming_responses_internal.h"
#include "apps/ikigai/providers/provider.h"
#include "shared/wrapper_json.h"
#include "vendor/yyjson/yyjson.h"
#include "openai_streaming_responses_events_test_helper.h"

START_TEST(test_function_call_arguments_done_is_noop) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    ik_openai_responses_stream_process_event(ctx, "response.function_call_arguments.done", "{}");
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_output_item_done_edge_cases) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    ik_openai_responses_stream_process_event(ctx,
                                             "response.output_item.added",
                                             "{\"item\":{\"type\":\"function_call\",\"call_id\":\"call_1\",\"name\":\"test\"},\"output_index\":0}");
    ik_openai_responses_stream_process_event(ctx, "response.output_item.done", "{}");
    ck_assert_int_eq((int)events->count, 2);

    events->count = 2;
    ik_openai_responses_stream_process_event(ctx, "response.output_item.done", "{\"output_index\":\"not an int\"}");
    ck_assert_int_eq((int)events->count, 2);

    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "response.output_item.done", "{\"output_index\":0}");
    ck_assert_int_eq((int)events->count, 1);

    ik_openai_responses_stream_process_event(ctx,
                                             "response.output_item.added",
                                             "{\"item\":{\"type\":\"function_call\",\"call_id\":\"call_1\",\"name\":\"test\"},\"output_index\":3}");
    ik_openai_responses_stream_process_event(ctx, "response.output_item.done", "{\"output_index\":3}");
    ck_assert_int_eq((int)events->count, 3);
    ck_assert_int_eq(events->items[2].type, IK_STREAM_TOOL_CALL_DONE);
}

END_TEST

START_TEST(test_response_completed_ends_tool_call) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    ik_openai_responses_stream_process_event(ctx,
                                             "response.output_item.added",
                                             "{\"item\":{\"type\":\"function_call\",\"call_id\":\"call_1\",\"name\":\"test\"},\"output_index\":0}");
    ik_openai_responses_stream_process_event(ctx, "response.completed", "{\"response\":{\"status\":\"completed\"}}");
    ck_assert_int_eq((int)events->count, 4);
    ck_assert_int_eq(events->items[2].type, IK_STREAM_TOOL_CALL_DONE);
    ck_assert_int_eq(events->items[3].type, IK_STREAM_DONE);
}
END_TEST

START_TEST(test_usage_and_model) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);
    ik_openai_responses_stream_process_event(ctx, "response.created",
                                             "{\"response\":{\"model\":\"gpt-4\"}}");
    ck_assert_str_eq(events->items[0].data.start.model, "gpt-4");
    ik_openai_responses_stream_process_event(ctx, "response.completed",
                                             "{\"response\":{\"status\":\"completed\",\"usage\":{\"input_tokens\":100,"
                                             "\"output_tokens\":50,\"total_tokens\":150,\"output_tokens_details\":"
                                             "{\"reasoning_tokens\":25}}}}");
    ck_assert_int_eq(events->items[1].data.done.usage.input_tokens, 100);
    ck_assert_int_eq(events->items[1].data.done.usage.thinking_tokens, 25);
}
END_TEST

START_TEST(test_usage_calc_and_err) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);
    ik_openai_responses_stream_process_event(ctx, "response.created", "{}");
    ik_openai_responses_stream_process_event(ctx, "response.completed",
                                             "{\"response\":{\"status\":\"completed\",\"usage\":{\"input_tokens\":100,"
                                             "\"output_tokens\":50}}}");
    ck_assert_int_eq(events->items[1].data.done.usage.total_tokens, 150);
    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "error",
                                             "{\"error\":{\"message\":\"Auth\",\"type\":\"authentication_error\"}}");
    ck_assert_int_eq(events->items[0].data.error.category, IK_ERR_CAT_AUTH);
}
END_TEST

START_TEST(test_incomplete_and_indices) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);
    ik_openai_responses_stream_process_event(ctx, "response.created", "{}");
    ik_openai_responses_stream_process_event(ctx, "response.output_text.delta",
                                             "{\"delta\":\"text\",\"content_index\":5}");
    ck_assert_int_eq(events->items[1].index, 5);
    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "response.reasoning_summary_text.delta",
                                             "{\"delta\":\"think\",\"summary_index\":7}");
    ck_assert_int_eq(events->items[0].index, 7);
    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "response.completed",
                                             "{\"response\":{\"status\":\"incomplete\",\"incomplete_details\":"
                                             "{\"reason\":\"max_tokens\"}}}");
    ck_assert_int_eq(events->items[0].type, IK_STREAM_DONE);
    ik_openai_responses_stream_process_event(ctx, "response.output_item.added",
                                             "{\"item\":{\"type\":\"function_call\",\"call_id\":\"c1\","
                                             "\"name\":\"fn\"},\"output_index\":2}");
    ik_openai_responses_stream_process_event(ctx, "response.function_call_arguments.delta",
                                             "{\"delta\":\"{}\",\"output_index\":10}");
    ck_assert_int_eq(events->items[events->count - 1].index, 10);
}
END_TEST

static Suite *openai_streaming_responses_events2_suite(void)
{
    Suite *s = suite_create("OpenAI Streaming Responses Events Part 2");

    TCase *tc = tcase_create("Events");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_function_call_arguments_done_is_noop);
    tcase_add_test(tc, test_output_item_done_edge_cases);
    tcase_add_test(tc, test_response_completed_ends_tool_call);
    tcase_add_test(tc, test_usage_and_model);
    tcase_add_test(tc, test_usage_calc_and_err);
    tcase_add_test(tc, test_incomplete_and_indices);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = openai_streaming_responses_events2_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/openai/openai_streaming_responses_events2_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
