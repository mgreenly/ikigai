#include "../../../test_constants.h"
/**
 * @file openai_streaming_responses_integration_test.c
 * @brief Integration tests for OpenAI Responses API streaming
 *
 * Tests complex scenarios including thinking, tool calls, completion, and errors.
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/openai/streaming.h"
#include "providers/openai/streaming_responses_internal.h"
#include "providers/provider.h"

/* ================================================================
 * Test Context and Event Capture
 * ================================================================ */

/**
 * Event collection for verification
 */
typedef struct {
    ik_stream_event_t *items;
    size_t count;
    size_t capacity;
} event_array_t;

static TALLOC_CTX *test_ctx;
static event_array_t *events;

/**
 * Stream callback - captures events for later verification
 */
static res_t stream_cb(const ik_stream_event_t *event, void *ctx)
{
    event_array_t *arr = (event_array_t *)ctx;

    /* Grow array if needed */
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

    /* Deep copy event */
    ik_stream_event_t *dst = &arr->items[arr->count];
    dst->type = event->type;
    dst->index = event->index;

    /* Copy variant data based on type */
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
            /* No additional data */
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

    /* Initialize event array */
    events = talloc_zero(test_ctx, event_array_t);
    events->items = NULL;
    events->count = 0;
    events->capacity = 0;
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * Integration Tests with process_event
 * ================================================================ */

START_TEST(test_write_callback_with_thinking_delta) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Set up model */
    char created[] = "event: response.created\n"
                     "data: {\"response\":{\"model\":\"gpt-4o\"}}\n\n";
    ik_openai_responses_stream_write_callback(created, 1, strlen(created), ctx);

    /* Thinking delta */
    char thinking[] = "event: response.reasoning_summary_text.delta\n"
                      "data: {\"delta\":\"Let me think\",\"summary_index\":0}\n\n";
    ik_openai_responses_stream_write_callback(thinking, 1, strlen(thinking), ctx);

    /* Should have START + THINKING_DELTA */
    ck_assert_int_eq((int)events->count, 2);
    ck_assert_int_eq(events->items[1].type, IK_STREAM_THINKING_DELTA);
    ck_assert_str_eq(events->items[1].data.delta.text, "Let me think");
}
END_TEST

START_TEST(test_write_callback_with_tool_call) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Set up model */
    char created[] = "event: response.created\n"
                     "data: {\"response\":{\"model\":\"gpt-4o\"}}\n\n";
    ik_openai_responses_stream_write_callback(created, 1, strlen(created), ctx);

    /* Tool call start */
    char tool_start[] = "event: response.output_item.added\n"
                        "data: {\"output_index\":0,\"item\":{\"type\":\"function_call\",\"call_id\":\"call_123\",\"name\":\"get_weather\"}}\n\n";
    ik_openai_responses_stream_write_callback(tool_start, 1, strlen(tool_start), ctx);

    /* Tool arguments delta */
    char tool_delta[] = "event: response.function_call_arguments.delta\n"
                        "data: {\"output_index\":0,\"delta\":\"{\\\"city\\\"\"}\n\n";
    ik_openai_responses_stream_write_callback(tool_delta, 1, strlen(tool_delta), ctx);

    /* Tool call done */
    char tool_done[] = "event: response.output_item.done\n"
                       "data: {\"output_index\":0}\n\n";
    ik_openai_responses_stream_write_callback(tool_done, 1, strlen(tool_done), ctx);

    /* Should have START + TOOL_START + TOOL_DELTA + TOOL_DONE */
    ck_assert_int_eq((int)events->count, 4);
    ck_assert_int_eq(events->items[1].type, IK_STREAM_TOOL_CALL_START);
    ck_assert_str_eq(events->items[1].data.tool_start.id, "call_123");
    ck_assert_str_eq(events->items[1].data.tool_start.name, "get_weather");
    ck_assert_int_eq(events->items[2].type, IK_STREAM_TOOL_CALL_DELTA);
    ck_assert_str_eq(events->items[2].data.tool_delta.arguments, "{\"city\"");
    ck_assert_int_eq(events->items[3].type, IK_STREAM_TOOL_CALL_DONE);
}

END_TEST

START_TEST(test_write_callback_with_completion) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Set up model */
    char created[] = "event: response.created\n"
                     "data: {\"response\":{\"model\":\"gpt-4o\"}}\n\n";
    ik_openai_responses_stream_write_callback(created, 1, strlen(created), ctx);

    /* Completion event */
    char completed[] = "event: response.completed\n"
                       "data: {\"response\":{\"status\":\"completed\",\"usage\":{\"input_tokens\":10,\"output_tokens\":20,\"total_tokens\":30}}}\n\n";
    ik_openai_responses_stream_write_callback(completed, 1, strlen(completed), ctx);

    /* Should have START + DONE */
    ck_assert_int_eq((int)events->count, 2);
    ck_assert_int_eq(events->items[1].type, IK_STREAM_DONE);
    ck_assert_int_eq(events->items[1].data.done.finish_reason, IK_FINISH_STOP);
    ck_assert_int_eq(events->items[1].data.done.usage.input_tokens, 10);
    ck_assert_int_eq(events->items[1].data.done.usage.output_tokens, 20);
    ck_assert_int_eq(events->items[1].data.done.usage.total_tokens, 30);

    /* Verify getters */
    ik_finish_reason_t reason = ik_openai_responses_stream_get_finish_reason(ctx);
    ck_assert_int_eq(reason, IK_FINISH_STOP);
}

END_TEST

START_TEST(test_write_callback_with_error_event) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Error event */
    char error_event[] = "event: error\n"
                         "data: {\"error\":{\"type\":\"rate_limit_error\",\"message\":\"Rate limit exceeded\"}}\n\n";
    ik_openai_responses_stream_write_callback(error_event, 1, strlen(error_event), ctx);

    /* Should emit ERROR event */
    ck_assert_int_eq((int)events->count, 1);
    ck_assert_int_eq(events->items[0].type, IK_STREAM_ERROR);
    ck_assert_int_eq(events->items[0].data.error.category, IK_ERR_CAT_RATE_LIMIT);
    ck_assert_str_eq(events->items[0].data.error.message, "Rate limit exceeded");
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *openai_streaming_responses_integration_suite(void)
{
    Suite *s = suite_create("OpenAI Streaming Responses Integration");

    /* Integration Tests */
    TCase *tc_integration = tcase_create("Integration");
    tcase_set_timeout(tc_integration, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_integration, setup, teardown);
    tcase_add_test(tc_integration, test_write_callback_with_thinking_delta);
    tcase_add_test(tc_integration, test_write_callback_with_tool_call);
    tcase_add_test(tc_integration, test_write_callback_with_completion);
    tcase_add_test(tc_integration, test_write_callback_with_error_event);
    suite_add_tcase(s, tc_integration);

    return s;
}

int main(void)
{
    Suite *s = openai_streaming_responses_integration_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/providers/openai/openai_streaming_responses_integration_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
