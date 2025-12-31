/**
 * @file openai_streaming_responses_events_branches_test.c
 * @brief Additional branch coverage tests for OpenAI Responses API event processing
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/openai/streaming.h"
#include "providers/openai/streaming_responses_internal.h"
#include "providers/provider.h"
#include "wrapper_json.h"
#include "vendor/yyjson/yyjson.h"
#include "openai_streaming_responses_events_test_helpers.c"

START_TEST(test_output_item_done_mismatched_index)
{
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    // Start a tool call at index 0
    ik_openai_responses_stream_process_event(ctx, "response.output_item.added",
        "{\"item\":{\"type\":\"function_call\",\"call_id\":\"call_1\",\"name\":\"test\"},\"output_index\":0}");
    ck_assert_int_eq((int)events->count, 2);
    ck_assert_int_eq(events->items[1].type, IK_STREAM_TOOL_CALL_START);

    // Try to end it with a different index (should not emit TOOL_CALL_DONE) - covers line 273
    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "response.output_item.done", "{\"output_index\":5}");
    ck_assert_int_eq((int)events->count, 0); // No event should be emitted
}
END_TEST

START_TEST(test_incomplete_details_missing_reason)
{
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);
    ik_openai_responses_stream_process_event(ctx, "response.created", "{}");

    // Test incomplete_details without reason field - covers lines 293, 296
    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "response.completed",
        "{\"response\":{\"status\":\"incomplete\",\"incomplete_details\":{}}}");
    ck_assert_int_eq((int)events->count, 1);
    ck_assert_int_eq(events->items[0].type, IK_STREAM_DONE);
}
END_TEST

START_TEST(test_error_message_null)
{
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    // Test with message_val being NULL - covers line 322
    ik_openai_responses_stream_process_event(ctx, "error",
        "{\"error\":{\"type\":\"server_error\"}}");
    ck_assert_int_eq((int)events->count, 1);
    ck_assert_str_eq(events->items[0].data.error.message, "Unknown error");
}
END_TEST

START_TEST(test_output_item_added_missing_fields)
{
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    // Test with type field missing entirely - covers line 205
    ik_openai_responses_stream_process_event(ctx, "response.output_item.added",
        "{\"item\":{}}");
    ck_assert_int_eq((int)events->count, 0);

    // Test with call_id field missing - covers lines 215, 218
    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "response.output_item.added",
        "{\"item\":{\"type\":\"function_call\",\"name\":\"test\"}}");
    ck_assert_int_eq((int)events->count, 0);

    // Test with name field missing - covers lines 216, 218
    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "response.output_item.added",
        "{\"item\":{\"type\":\"function_call\",\"call_id\":\"call_1\"}}");
    ck_assert_int_eq((int)events->count, 0);
}
END_TEST

START_TEST(test_function_call_args_when_not_in_tool_call)
{
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    // Test function_call_arguments.delta when NOT in a tool call - covers line 248
    ik_openai_responses_stream_process_event(ctx, "response.function_call_arguments.delta",
        "{\"delta\":\"args\"}");
    ck_assert_int_eq((int)events->count, 0); // Should not emit event
}
END_TEST

START_TEST(test_text_delta_with_empty_string)
{
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    // Empty string delta should still emit event (not the false branch we want)
    ik_openai_responses_stream_process_event(ctx, "response.output_text.delta",
        "{\"delta\":\"\"}");
    // This actually covers the TRUE branch of line 162 since empty string is still valid
    ck_assert_int_eq((int)events->count, 2); // START + TEXT_DELTA
}
END_TEST

START_TEST(test_thinking_delta_with_empty_string)
{
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    // Empty string delta should still emit event
    ik_openai_responses_stream_process_event(ctx, "response.reasoning_summary_text.delta",
        "{\"delta\":\"\"}");
    ck_assert_int_eq((int)events->count, 2); // START + THINKING_DELTA
}
END_TEST

static Suite *openai_streaming_responses_events_branches_suite(void)
{
    Suite *s = suite_create("OpenAI Streaming Responses Events Branches");

    TCase *tc = tcase_create("Branches");
    tcase_set_timeout(tc, 30);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_output_item_done_mismatched_index);
    tcase_add_test(tc, test_incomplete_details_missing_reason);
    tcase_add_test(tc, test_error_message_null);
    tcase_add_test(tc, test_output_item_added_missing_fields);
    tcase_add_test(tc, test_function_call_args_when_not_in_tool_call);
    tcase_add_test(tc, test_text_delta_with_number_value);
    tcase_add_test(tc, test_thinking_delta_with_number_value);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = openai_streaming_responses_events_branches_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
