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


static Suite *openai_streaming_responses_events2_suite(void)
{
    Suite *s = suite_create("OpenAI Streaming Responses Events Part 2");

    TCase *tc = tcase_create("Events");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_function_call_arguments_done_is_noop);
    tcase_add_test(tc, test_output_item_done_edge_cases);
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
