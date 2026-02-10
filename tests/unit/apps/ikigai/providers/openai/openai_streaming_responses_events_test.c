#include "tests/test_constants.h"
/**
 * @file openai_streaming_responses_events_test.c
 * @brief Tests for OpenAI Responses API event processing edge cases (part 1)
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

START_TEST(test_invalid_json) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    ik_openai_responses_stream_process_event(ctx, "response.created", "invalid json");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "response.created", "[]");
    ck_assert_int_eq((int)events->count, 0);
}
END_TEST

START_TEST(test_text_delta_edge_cases) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    ik_openai_responses_stream_process_event(ctx, "response.output_text.delta", "{}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "response.output_text.delta", "{\"delta\":123}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "response.output_text.delta", "{\"delta\":null}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "response.output_text.delta", "{\"delta\":\"text\"}");
    ck_assert_int_eq((int)events->count, 2);
    ck_assert_int_eq(events->items[1].index, 0);

    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "response.output_text.delta",
                                             "{\"delta\":\"text\",\"content_index\":\"not an int\"}");
    ck_assert_int_eq(events->items[0].index, 0);
}

END_TEST

START_TEST(test_thinking_delta_edge_cases) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    ik_openai_responses_stream_process_event(ctx, "response.reasoning_summary_text.delta", "{}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "response.reasoning_summary_text.delta", "{\"delta\":123}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "response.reasoning_summary_text.delta", "{\"delta\":null}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "response.reasoning_summary_text.delta", "{\"delta\":\"thinking\"}");
    ck_assert_int_eq((int)events->count, 2);
    ck_assert_int_eq(events->items[1].type, IK_STREAM_THINKING_DELTA);
    ck_assert_int_eq(events->items[1].index, 0);

    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "response.reasoning_summary_text.delta",
                                             "{\"delta\":\"thinking\",\"summary_index\":\"not an int\"}");
    ck_assert_int_eq(events->items[0].index, 0);
}

END_TEST

START_TEST(test_output_item_added_edge_cases) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    ik_openai_responses_stream_process_event(ctx, "response.output_item.added", "{}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "response.output_item.added", "{\"item\":\"not an object\"}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "response.output_item.added", "{\"item\":{\"type\":null}}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "response.output_item.added", "{\"item\":{\"type\":\"text\"}}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx,
                                             "response.output_item.added",
                                             "{\"item\":{\"type\":\"function_call\",\"call_id\":\"call_123\",\"name\":\"test\"}}");
    ck_assert_int_eq((int)events->count, 2);
    ck_assert_int_eq(events->items[1].index, 0);

    events->count = 0;
    ik_openai_responses_stream_process_event(ctx,
                                             "response.output_item.added",
                                             "{\"item\":{\"type\":\"function_call\",\"call_id\":\"call_123\",\"name\":\"test\"},\"output_index\":\"not an int\"}");
    ck_assert_int_eq(events->items[0].index, 0);

    events->count = 0;
    ik_openai_responses_stream_process_event(ctx,
                                             "response.output_item.added",
                                             "{\"item\":{\"type\":\"function_call\",\"call_id\":null,\"name\":\"test\"}}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx,
                                             "response.output_item.added",
                                             "{\"item\":{\"type\":\"function_call\",\"call_id\":\"call_123\",\"name\":null}}");
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_output_item_added_ends_previous_tool_call) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    ik_openai_responses_stream_process_event(ctx,
                                             "response.output_item.added",
                                             "{\"item\":{\"type\":\"function_call\",\"call_id\":\"call_1\",\"name\":\"test1\"},\"output_index\":0}");
    ck_assert_int_eq((int)events->count, 2);

    ik_openai_responses_stream_process_event(ctx,
                                             "response.output_item.added",
                                             "{\"item\":{\"type\":\"function_call\",\"call_id\":\"call_2\",\"name\":\"test2\"},\"output_index\":1}");
    ck_assert_int_eq((int)events->count, 4);
    ck_assert_int_eq(events->items[2].type, IK_STREAM_TOOL_CALL_DONE);
    ck_assert_int_eq(events->items[3].type, IK_STREAM_TOOL_CALL_START);
}

END_TEST

START_TEST(test_function_call_arguments_delta_edge_cases) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    ik_openai_responses_stream_process_event(ctx, "response.function_call_arguments.delta", "{}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "response.function_call_arguments.delta", "{\"delta\":123}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "response.function_call_arguments.delta", "{\"delta\":null}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx, "response.function_call_arguments.delta", "{\"delta\":\"{}\"}");
    ck_assert_int_eq((int)events->count, 0);

    ik_openai_responses_stream_process_event(ctx,
                                             "response.output_item.added",
                                             "{\"item\":{\"type\":\"function_call\",\"call_id\":\"call_1\",\"name\":\"test\"},\"output_index\":5}");
    ik_openai_responses_stream_process_event(ctx, "response.function_call_arguments.delta", "{\"delta\":\"{}\"}");
    ck_assert_int_eq((int)events->count, 3);
    ck_assert_int_eq(events->items[2].index, 5);

    events->count = 2;
    ik_openai_responses_stream_process_event(ctx, "response.function_call_arguments.delta",
                                             "{\"delta\":\"{}\",\"output_index\":\"not an int\"}");
    ck_assert_int_eq(events->items[2].index, 5);
}

END_TEST
static Suite *openai_streaming_responses_events_suite(void)
{
    Suite *s = suite_create("OpenAI Streaming Responses Events Part 1");

    TCase *tc = tcase_create("Events");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_invalid_json);
    tcase_add_test(tc, test_text_delta_edge_cases);
    tcase_add_test(tc, test_thinking_delta_edge_cases);
    tcase_add_test(tc, test_output_item_added_edge_cases);
    tcase_add_test(tc, test_output_item_added_ends_previous_tool_call);
    tcase_add_test(tc, test_function_call_arguments_delta_edge_cases);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = openai_streaming_responses_events_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/openai/openai_streaming_responses_events_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
