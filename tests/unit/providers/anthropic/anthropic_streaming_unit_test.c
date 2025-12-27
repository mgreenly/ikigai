/**
 * @file anthropic_streaming_unit_test.c
 * @brief Unit tests for Anthropic streaming utility functions
 *
 * Tests the low-level functions in streaming.c:
 * - ik_anthropic_stream_get_usage()
 * - ik_anthropic_stream_get_finish_reason()
 * - ik_anthropic_stream_process_event() edge cases
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/anthropic/streaming.h"
#include "providers/provider.h"

static TALLOC_CTX *test_ctx;
static ik_anthropic_stream_ctx_t *stream_ctx;

/* Test event capture */
#define MAX_EVENTS 16
static ik_stream_event_t captured_events[MAX_EVENTS];
static size_t captured_count;

/* ================================================================
 * Callbacks
 * ================================================================ */

static res_t test_stream_cb(const ik_stream_event_t *event, void *ctx)
{
    (void)ctx;

    if (captured_count < MAX_EVENTS) {
        /* Deep copy event */
        captured_events[captured_count] = *event;

        /* Copy string data if present */
        if (event->type == IK_STREAM_ERROR && event->data.error.message) {
            captured_events[captured_count].data.error.message =
                talloc_strdup(test_ctx, event->data.error.message);
        }

        captured_count++;
    }

    return OK(NULL);
}

/* ================================================================
 * Fixtures
 * ================================================================ */

static void setup(void)
{
    test_ctx = talloc_new(NULL);

    /* Create streaming context */
    res_t r = ik_anthropic_stream_ctx_create(test_ctx, test_stream_cb, NULL, &stream_ctx);
    ck_assert(!is_err(&r));

    /* Reset event capture */
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * Getter Tests
 * ================================================================ */

START_TEST(test_get_usage_initial) {
    /* Get usage from fresh context */
    ik_usage_t usage = ik_anthropic_stream_get_usage(stream_ctx);

    /* Should be all zeros initially */
    ck_assert_int_eq(usage.input_tokens, 0);
    ck_assert_int_eq(usage.output_tokens, 0);
    ck_assert_int_eq(usage.thinking_tokens, 0);
    ck_assert_int_eq(usage.total_tokens, 0);
}
END_TEST START_TEST(test_get_usage_after_update)
{
    /* Manually update usage to simulate stream processing */
    stream_ctx->usage.input_tokens = 100;
    stream_ctx->usage.output_tokens = 50;
    stream_ctx->usage.thinking_tokens = 25;
    stream_ctx->usage.total_tokens = 175;

    /* Get usage */
    ik_usage_t usage = ik_anthropic_stream_get_usage(stream_ctx);

    /* Should return the updated values */
    ck_assert_int_eq(usage.input_tokens, 100);
    ck_assert_int_eq(usage.output_tokens, 50);
    ck_assert_int_eq(usage.thinking_tokens, 25);
    ck_assert_int_eq(usage.total_tokens, 175);
}

END_TEST START_TEST(test_get_finish_reason_initial)
{
    /* Get finish reason from fresh context */
    ik_finish_reason_t reason = ik_anthropic_stream_get_finish_reason(stream_ctx);

    /* Should be UNKNOWN initially */
    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}

END_TEST START_TEST(test_get_finish_reason_after_update)
{
    /* Manually update finish reason to simulate stream processing */
    stream_ctx->finish_reason = IK_FINISH_STOP;

    /* Get finish reason */
    ik_finish_reason_t reason = ik_anthropic_stream_get_finish_reason(stream_ctx);

    /* Should return the updated value */
    ck_assert_int_eq(reason, IK_FINISH_STOP);
}

END_TEST
/* ================================================================
 * Event Processing Edge Case Tests
 * ================================================================ */

START_TEST(test_process_ping_event)
{
    /* Process ping event - should be ignored */
    ik_anthropic_stream_process_event(stream_ctx, "ping", "{}");

    /* Should not generate any stream events */
    ck_assert_int_eq((int)captured_count, 0);
}

END_TEST START_TEST(test_process_invalid_json)
{
    /* Process event with invalid JSON */
    ik_anthropic_stream_process_event(stream_ctx, "message_start", "not json at all");

    /* Should generate error event */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_ERROR);
    ck_assert_int_eq(captured_events[0].data.error.category, IK_ERR_CAT_UNKNOWN);
    ck_assert_str_eq(captured_events[0].data.error.message, "Invalid JSON in SSE event");
}

END_TEST START_TEST(test_process_non_object_json)
{
    /* Process event with JSON that's not an object */
    ik_anthropic_stream_process_event(stream_ctx, "message_start", "\"just a string\"");

    /* Should generate error event */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_ERROR);
    ck_assert_int_eq(captured_events[0].data.error.category, IK_ERR_CAT_UNKNOWN);
    ck_assert_str_eq(captured_events[0].data.error.message, "SSE event data is not a JSON object");
}

END_TEST START_TEST(test_process_non_object_json_array)
{
    /* Process event with JSON array instead of object */
    ik_anthropic_stream_process_event(stream_ctx, "message_start", "[1, 2, 3]");

    /* Should generate error event */
    ck_assert_int_eq((int)captured_count, 1);
    ck_assert_int_eq(captured_events[0].type, IK_STREAM_ERROR);
    ck_assert_int_eq(captured_events[0].data.error.category, IK_ERR_CAT_UNKNOWN);
    ck_assert_str_eq(captured_events[0].data.error.message, "SSE event data is not a JSON object");
}

END_TEST START_TEST(test_process_error_event)
{
    /* Process error event with minimal error structure */
    const char *error_json = "{"
                             "\"type\": \"error\","
                             "\"error\": {"
                             "\"type\": \"invalid_request_error\","
                             "\"message\": \"Test error message\""
                             "}"
                             "}";

    ik_anthropic_stream_process_event(stream_ctx, "error", error_json);

    /* Should generate error stream event */
    ck_assert(captured_count >= 1);

    /* Find the error event (might not be first if there are other events) */
    bool found_error = false;
    for (size_t i = 0; i < captured_count; i++) {
        if (captured_events[i].type == IK_STREAM_ERROR) {
            found_error = true;
            ck_assert_ptr_nonnull(captured_events[i].data.error.message);
            break;
        }
    }
    ck_assert(found_error);
}

END_TEST START_TEST(test_process_unknown_event)
{
    /* Process unknown event type - should be silently ignored */
    const char *unknown_json = "{\"type\": \"unknown_event\"}";

    ik_anthropic_stream_process_event(stream_ctx, "unknown_future_event", unknown_json);

    /* Should not generate any error - unknown events are silently ignored */
    /* Only check that no error event was generated */
    for (size_t i = 0; i < captured_count; i++) {
        ck_assert_int_ne(captured_events[i].type, IK_STREAM_ERROR);
    }
}

END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *anthropic_streaming_unit_suite(void)
{
    Suite *s = suite_create("Anthropic Streaming Unit");

    TCase *tc_getters = tcase_create("Getter Functions");
    tcase_add_unchecked_fixture(tc_getters, setup, teardown);
    tcase_add_test(tc_getters, test_get_usage_initial);
    tcase_add_test(tc_getters, test_get_usage_after_update);
    tcase_add_test(tc_getters, test_get_finish_reason_initial);
    tcase_add_test(tc_getters, test_get_finish_reason_after_update);
    suite_add_tcase(s, tc_getters);

    TCase *tc_edge_cases = tcase_create("Event Processing Edge Cases");
    tcase_add_checked_fixture(tc_edge_cases, setup, teardown);
    tcase_add_test(tc_edge_cases, test_process_ping_event);
    tcase_add_test(tc_edge_cases, test_process_invalid_json);
    tcase_add_test(tc_edge_cases, test_process_non_object_json);
    tcase_add_test(tc_edge_cases, test_process_non_object_json_array);
    tcase_add_test(tc_edge_cases, test_process_error_event);
    tcase_add_test(tc_edge_cases, test_process_unknown_event);
    suite_add_tcase(s, tc_edge_cases);

    return s;
}

int main(void)
{
    Suite *s = anthropic_streaming_unit_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
