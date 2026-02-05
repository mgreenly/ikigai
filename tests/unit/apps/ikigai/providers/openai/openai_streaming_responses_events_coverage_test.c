#include "tests/test_constants.h"
/**
 * @file openai_streaming_responses_events_coverage_test.c
 * @brief Additional coverage tests for OpenAI Responses API event processing
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/providers/openai/streaming.h"
#include "apps/ikigai/providers/openai/streaming_responses_internal.h"
#include "apps/ikigai/providers/provider.h"
#include "shared/wrapper_json.h"
#include "vendor/yyjson/yyjson.h"

typedef struct {
    ik_stream_event_t *items;
    size_t count;
    size_t capacity;
} event_array_t;

static TALLOC_CTX *test_ctx;
static event_array_t *events;

static res_t stream_cb(const ik_stream_event_t *event, void *ctx)
{
    event_array_t *arr = (event_array_t *)ctx;

    if (arr->count >= arr->capacity) {
        size_t new_capacity = arr->capacity == 0 ? 8 : arr->capacity * 2;
        ik_stream_event_t *new_items = talloc_realloc(test_ctx, arr->items,
                                                      ik_stream_event_t, (unsigned int)new_capacity);
        if (new_items == NULL) {
            return ERR(test_ctx, OUT_OF_MEMORY, "Failed to grow event array");
        }
        arr->items = new_items;
        arr->capacity = new_capacity;
    }

    ik_stream_event_t *dst = &arr->items[arr->count];
    dst->type = event->type;
    dst->index = event->index;

    switch (event->type) {
        case IK_STREAM_START:
            dst->data.start.model = event->data.start.model
                ? talloc_strdup(arr->items, event->data.start.model)
                : NULL;
            break;
        case IK_STREAM_TEXT_DELTA:
        case IK_STREAM_THINKING_DELTA:
            dst->data.delta.text = event->data.delta.text
                ? talloc_strdup(arr->items, event->data.delta.text)
                : NULL;
            break;
        case IK_STREAM_TOOL_CALL_START:
            dst->data.tool_start.id = event->data.tool_start.id
                ? talloc_strdup(arr->items, event->data.tool_start.id)
                : NULL;
            dst->data.tool_start.name = event->data.tool_start.name
                ? talloc_strdup(arr->items, event->data.tool_start.name)
                : NULL;
            break;
        case IK_STREAM_TOOL_CALL_DELTA:
            dst->data.tool_delta.arguments = event->data.tool_delta.arguments
                ? talloc_strdup(arr->items, event->data.tool_delta.arguments)
                : NULL;
            break;
        case IK_STREAM_TOOL_CALL_DONE:
            break;
        case IK_STREAM_DONE:
            dst->data.done.finish_reason = event->data.done.finish_reason;
            dst->data.done.usage = event->data.done.usage;
            dst->data.done.provider_data = event->data.done.provider_data
                ? talloc_strdup(arr->items, event->data.done.provider_data)
                : NULL;
            break;
        case IK_STREAM_ERROR:
            dst->data.error.category = event->data.error.category;
            dst->data.error.message = event->data.error.message
                ? talloc_strdup(arr->items, event->data.error.message)
                : NULL;
            break;
    }

    arr->count++;
    return OK(NULL);
}

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    events = talloc_zero(test_ctx, event_array_t);
    events->items = NULL;
    events->count = 0;
    events->capacity = 0;
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

START_TEST(test_usage_edge_cases) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);
    ik_openai_responses_stream_process_event(ctx, "response.created", "{}");

    // Test usage is not an object
    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "response.completed",
                                             "{\"response\":{\"status\":\"completed\",\"usage\":\"not an object\"}}");
    ck_assert_int_eq(events->items[0].data.done.usage.input_tokens, 0);

    // Test input_tokens is not an int
    events->count = 0;
    ik_openai_responses_stream_process_event(ctx,
                                             "response.completed",
                                             "{\"response\":{\"status\":\"completed\",\"usage\":{\"input_tokens\":\"not an int\"}}}");
    ck_assert_int_eq(events->items[0].data.done.usage.input_tokens, 0);

    // Test output_tokens is not an int
    events->count = 0;
    ik_openai_responses_stream_process_event(ctx,
                                             "response.completed",
                                             "{\"response\":{\"status\":\"completed\",\"usage\":{\"output_tokens\":\"not an int\"}}}");
    ck_assert_int_eq(events->items[0].data.done.usage.output_tokens, 0);

    // Test total_tokens is not an int (should calculate from input+output)
    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "response.completed",
                                             "{\"response\":{\"status\":\"completed\",\"usage\":{\"input_tokens\":100,"
                                             "\"output_tokens\":50,\"total_tokens\":\"not an int\"}}}");
    ck_assert_int_eq(events->items[0].data.done.usage.total_tokens, 150);

    // Test output_tokens_details is not an object
    events->count = 0;
    ik_openai_responses_stream_process_event(ctx,
                                             "response.completed",
                                             "{\"response\":{\"status\":\"completed\",\"usage\":{\"output_tokens_details\":\"not an object\"}}}");
    ck_assert_int_eq(events->items[0].data.done.usage.thinking_tokens, 0);

    // Test reasoning_tokens is not an int
    events->count = 0;
    ik_openai_responses_stream_process_event(ctx,
                                             "response.completed",
                                             "{\"response\":{\"status\":\"completed\",\"usage\":{\"output_tokens_details\":"
                                             "{\"reasoning_tokens\":\"not an int\"}}}}");
    ck_assert_int_eq(events->items[0].data.done.usage.thinking_tokens, 0);

    // Test total_tokens is NULL with input/output tokens as 0 (else branch of line 104)
    talloc_free(ctx);
    events->count = 0;
    ctx = ik_openai_responses_stream_ctx_create(test_ctx, stream_cb, events);
    ik_openai_responses_stream_process_event(ctx, "response.created", "{}");
    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "response.completed",
                                             "{\"response\":{\"status\":\"completed\",\"usage\":{}}}");
    ck_assert_int_eq(events->items[0].data.done.usage.total_tokens, 0);

    // Test reasoning_tokens is NULL
    talloc_free(ctx);
    events->count = 0;
    ctx = ik_openai_responses_stream_ctx_create(test_ctx, stream_cb, events);
    ik_openai_responses_stream_process_event(ctx, "response.created", "{}");
    events->count = 0;
    ik_openai_responses_stream_process_event(ctx,
                                             "response.completed",
                                             "{\"response\":{\"status\":\"completed\",\"usage\":{\"output_tokens_details\":{}}}}");
    ck_assert_int_eq(events->items[0].data.done.usage.thinking_tokens, 0);

    // Test input_tokens is NULL
    talloc_free(ctx);
    events->count = 0;
    ctx = ik_openai_responses_stream_ctx_create(test_ctx, stream_cb, events);
    ik_openai_responses_stream_process_event(ctx, "response.created", "{}");
    events->count = 0;
    ik_openai_responses_stream_process_event(ctx,
                                             "response.completed",
                                             "{\"response\":{\"status\":\"completed\",\"usage\":{\"output_tokens\":50}}}");
    ck_assert_int_eq(events->items[0].data.done.usage.input_tokens, 0);

    // Test output_tokens is NULL
    talloc_free(ctx);
    events->count = 0;
    ctx = ik_openai_responses_stream_ctx_create(test_ctx, stream_cb, events);
    ik_openai_responses_stream_process_event(ctx, "response.created", "{}");
    events->count = 0;
    ik_openai_responses_stream_process_event(ctx,
                                             "response.completed",
                                             "{\"response\":{\"status\":\"completed\",\"usage\":{\"input_tokens\":100}}}");
    ck_assert_int_eq(events->items[0].data.done.usage.output_tokens, 0);

    // Test total_tokens is NULL (but should be calculated because input > 0)
    talloc_free(ctx);
    events->count = 0;
    ctx = ik_openai_responses_stream_ctx_create(test_ctx, stream_cb, events);
    ik_openai_responses_stream_process_event(ctx, "response.created", "{}");
    events->count = 0;
    ik_openai_responses_stream_process_event(ctx,
                                             "response.completed",
                                             "{\"response\":{\"status\":\"completed\",\"usage\":{\"input_tokens\":100,\"output_tokens\":0}}}");
    ck_assert_int_eq(events->items[0].data.done.usage.total_tokens, 100);
}
END_TEST

START_TEST(test_error_event_types) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    // Test rate_limit_error
    ik_openai_responses_stream_process_event(ctx, "error",
                                             "{\"error\":{\"message\":\"Rate limited\",\"type\":\"rate_limit_error\"}}");
    ck_assert_int_eq((int)events->count, 1);
    ck_assert_int_eq(events->items[0].data.error.category, IK_ERR_CAT_RATE_LIMIT);

    // Test invalid_request_error
    events->count = 0;
    ik_openai_responses_stream_process_event(ctx,
                                             "error",
                                             "{\"error\":{\"message\":\"Invalid request\",\"type\":\"invalid_request_error\"}}");
    ck_assert_int_eq(events->items[0].data.error.category, IK_ERR_CAT_INVALID_ARG);

    // Test server_error
    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "error",
                                             "{\"error\":{\"message\":\"Server error\",\"type\":\"server_error\"}}");
    ck_assert_int_eq(events->items[0].data.error.category, IK_ERR_CAT_SERVER);

    // Test unknown error type
    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "error",
                                             "{\"error\":{\"message\":\"Unknown\",\"type\":\"unknown_type\"}}");
    ck_assert_int_eq(events->items[0].data.error.category, IK_ERR_CAT_UNKNOWN);

    // Test NULL type
    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "error",
                                             "{\"error\":{\"message\":\"Error without type\"}}");
    ck_assert_int_eq(events->items[0].data.error.category, IK_ERR_CAT_UNKNOWN);

    // Test NULL message
    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "error",
                                             "{\"error\":{\"type\":\"server_error\"}}");
    ck_assert_str_eq(events->items[0].data.error.message, "Unknown error");

    // Test error_val is NULL
    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "error", "{}");
    ck_assert_int_eq((int)events->count, 0);

    // Test error_val is not an object
    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "error", "{\"error\":\"not an object\"}");
    ck_assert_int_eq((int)events->count, 0);
}
END_TEST

START_TEST(test_response_completed_edge_cases) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);
    ik_openai_responses_stream_process_event(ctx, "response.created", "{}");

    // Test response is not an object
    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "response.completed", "{\"response\":\"not an object\"}");
    ck_assert_int_eq((int)events->count, 1);

    // Test status is NULL
    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "response.completed", "{\"response\":{}}");
    ck_assert_int_eq((int)events->count, 1);

    // Test incomplete_details is not an object
    events->count = 0;
    ik_openai_responses_stream_process_event(ctx,
                                             "response.completed",
                                             "{\"response\":{\"status\":\"incomplete\",\"incomplete_details\":\"not an object\"}}");
    ck_assert_int_eq((int)events->count, 1);
}
END_TEST

START_TEST(test_helper_function_branches) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    // Test maybe_emit_start when already started (line 47 false branch)
    ik_openai_responses_stream_process_event(ctx, "response.created", "{}");
    ck_assert_int_eq((int)events->count, 1);
    events->count = 0;
    ik_openai_responses_stream_process_event(ctx, "response.output_text.delta", "{\"delta\":\"text\"}");
    ck_assert_int_eq((int)events->count, 1); // Only text delta, no new START event
    ck_assert_int_eq(events->items[0].type, IK_STREAM_TEXT_DELTA);

    // Test maybe_end_tool_call when not in a tool call (line 65 false branch)
    talloc_free(ctx);
    events->count = 0;
    ctx = ik_openai_responses_stream_ctx_create(test_ctx, stream_cb, events);
    ik_openai_responses_stream_process_event(ctx, "response.created", "{}");
    ik_openai_responses_stream_process_event(ctx, "response.completed", "{\"response\":{\"status\":\"completed\"}}");
    // Should emit START and DONE, but no TOOL_CALL_DONE
    ck_assert_int_eq((int)events->count, 2);
    ck_assert_int_eq(events->items[0].type, IK_STREAM_START);
    ck_assert_int_eq(events->items[1].type, IK_STREAM_DONE);
}
END_TEST

static Suite *openai_streaming_responses_events_coverage_suite(void)
{
    Suite *s = suite_create("OpenAI Streaming Responses Events Coverage");

    TCase *tc = tcase_create("Coverage");
    tcase_set_timeout(tc, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_usage_edge_cases);
    tcase_add_test(tc, test_error_event_types);
    tcase_add_test(tc, test_response_completed_edge_cases);
    tcase_add_test(tc, test_helper_function_branches);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = openai_streaming_responses_events_coverage_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/openai/openai_streaming_responses_events_coverage_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
