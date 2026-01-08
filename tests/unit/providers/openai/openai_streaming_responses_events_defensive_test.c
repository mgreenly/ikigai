/**
 * @file openai_streaming_responses_events_defensive_test.c
 * @brief Defensive code coverage tests for OpenAI Responses API event processing
 *
 * Tests defensive NULL checks on yyjson_get_str() return values using mocking.
 * These branches are unlikely to occur in practice but are included for robustness.
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

/* Mock state */
static bool mock_yyjson_get_str_should_return_null = false;
static int mock_call_count = 0;
static int mock_return_null_on_call = -1;

/* Mock yyjson_get_str_ to return NULL when configured */
const char *yyjson_get_str_(yyjson_val *val)
{
    if (mock_yyjson_get_str_should_return_null) {
        return NULL;
    }

    /* Return NULL on specific call number */
    if (mock_return_null_on_call >= 0) {
        mock_call_count++;
        if (mock_call_count == mock_return_null_on_call) {
            return NULL;
        }
    }

    return yyjson_get_str(val);
}

static void reset_mock(void)
{
    mock_yyjson_get_str_should_return_null = false;
    mock_call_count = 0;
    mock_return_null_on_call = -1;
}

START_TEST(test_text_delta_yyjson_get_str_returns_null) {
    reset_mock();
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Enable mock to return NULL - covers line 162 (delta == NULL) */
    mock_yyjson_get_str_should_return_null = true;

    /* Even with valid JSON, mock makes yyjson_get_str return NULL */
    ik_openai_responses_stream_process_event(ctx, "response.output_text.delta",
                                             "{\"delta\":\"text\"}");

    /* No events should be emitted when delta is NULL */
    ck_assert_int_eq((int)events->count, 0);
}
END_TEST

START_TEST(test_thinking_delta_yyjson_get_str_returns_null) {
    reset_mock();
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Enable mock to return NULL - covers line 184 (delta == NULL) */
    mock_yyjson_get_str_should_return_null = true;

    ik_openai_responses_stream_process_event(ctx, "response.reasoning_summary_text.delta",
                                             "{\"delta\":\"thinking\"}");

    /* No events should be emitted when delta is NULL */
    ck_assert_int_eq((int)events->count, 0);
}
END_TEST

START_TEST(test_function_call_args_delta_yyjson_get_str_returns_null) {
    reset_mock();
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Start a tool call first */
    ik_openai_responses_stream_process_event(ctx,
                                             "response.output_item.added",
                                             "{\"item\":{\"type\":\"function_call\",\"call_id\":\"call_1\",\"name\":\"test\"},\"output_index\":0}");
    events->count = 0;

    /* Enable mock to return NULL - covers line 248 (delta == NULL) */
    mock_yyjson_get_str_should_return_null = true;

    ik_openai_responses_stream_process_event(ctx, "response.function_call_arguments.delta",
                                             "{\"delta\":\"args\"}");

    /* No events should be emitted when delta is NULL */
    ck_assert_int_eq((int)events->count, 0);
}
END_TEST

START_TEST(test_output_item_added_call_id_yyjson_get_str_returns_null) {
    reset_mock();
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Mock returns NULL on first call (call_id) - covers line 218 (call_id == NULL) */
    mock_return_null_on_call = 1;

    ik_openai_responses_stream_process_event(ctx,
                                             "response.output_item.added",
                                             "{\"item\":{\"type\":\"function_call\",\"call_id\":\"call_1\",\"name\":\"test\"},\"output_index\":0}");

    /* No TOOL_CALL_START event should be emitted when call_id is NULL */
    ck_assert_int_eq((int)events->count, 0);
}
END_TEST

START_TEST(test_response_completed_incomplete_reason_yyjson_get_str_returns_null) {
    reset_mock();
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);
    ik_openai_responses_stream_process_event(ctx, "response.created", "{}");
    events->count = 0;

    /* Mock returns NULL for reason field - covers line 297 (yyjson_get_str returns NULL) */
    mock_return_null_on_call = 1;

    ik_openai_responses_stream_process_event(ctx,
                                             "response.completed",
                                             "{\"response\":{\"status\":\"incomplete\",\"incomplete_details\":{\"reason\":\"max_tokens\"}}}");

    /* Event should still be emitted, incomplete_reason will be NULL */
    ck_assert_int_eq((int)events->count, 1);
    ck_assert_int_eq(events->items[0].type, IK_STREAM_DONE);
}
END_TEST

START_TEST(test_error_message_yyjson_get_str_returns_null) {
    reset_mock();
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Mock returns NULL on first call (message) - covers line 325 (message == NULL) */
    mock_return_null_on_call = 1;

    ik_openai_responses_stream_process_event(ctx, "error",
                                             "{\"error\":{\"message\":\"Error occurred\",\"type\":\"server_error\"}}");

    /* Error event should be emitted with fallback message */
    ck_assert_int_eq((int)events->count, 1);
    ck_assert_int_eq(events->items[0].type, IK_STREAM_ERROR);
    ck_assert_str_eq(events->items[0].data.error.message, "Unknown error");
}
END_TEST

static Suite *openai_streaming_responses_events_defensive_suite(void)
{
    Suite *s = suite_create("OpenAI Streaming Responses Events Defensive");

    TCase *tc = tcase_create("Defensive");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_text_delta_yyjson_get_str_returns_null);
    tcase_add_test(tc, test_thinking_delta_yyjson_get_str_returns_null);
    tcase_add_test(tc, test_function_call_args_delta_yyjson_get_str_returns_null);
    tcase_add_test(tc, test_output_item_added_call_id_yyjson_get_str_returns_null);
    tcase_add_test(tc, test_response_completed_incomplete_reason_yyjson_get_str_returns_null);
    tcase_add_test(tc, test_error_message_yyjson_get_str_returns_null);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = openai_streaming_responses_events_defensive_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
