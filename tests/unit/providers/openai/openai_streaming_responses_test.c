/**
 * @file openai_streaming_responses_test.c
 * @brief Tests for OpenAI Responses API streaming implementation
 *
 * Tests context creation, getters, and basic write callback functionality.
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
 * Context Creation and Getters Tests
 * ================================================================ */

START_TEST(test_ctx_create_initializes_correctly) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Verify initialization */
    ck_assert_ptr_nonnull(ctx);
    ck_assert_ptr_eq(ctx->stream_cb, stream_cb);
    ck_assert_ptr_eq(ctx->stream_ctx, events);
    ck_assert_ptr_null(ctx->model);
    ck_assert_int_eq(ctx->finish_reason, IK_FINISH_UNKNOWN);
    ck_assert_int_eq(ctx->usage.input_tokens, 0);
    ck_assert_int_eq(ctx->usage.output_tokens, 0);
    ck_assert_int_eq(ctx->usage.thinking_tokens, 0);
    ck_assert_int_eq(ctx->usage.total_tokens, 0);
    ck_assert(!ctx->started);
    ck_assert(!ctx->in_tool_call);
    ck_assert_int_eq(ctx->tool_call_index, -1);
    ck_assert_ptr_null(ctx->current_tool_id);
    ck_assert_ptr_null(ctx->current_tool_name);
    ck_assert_ptr_nonnull(ctx->sse_parser);
}
END_TEST

START_TEST(test_get_usage_returns_zero_initially) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    ik_usage_t usage = ik_openai_responses_stream_get_usage(ctx);

    ck_assert_int_eq(usage.input_tokens, 0);
    ck_assert_int_eq(usage.output_tokens, 0);
    ck_assert_int_eq(usage.thinking_tokens, 0);
    ck_assert_int_eq(usage.total_tokens, 0);
}

END_TEST

START_TEST(test_get_finish_reason_returns_unknown_initially) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    ik_finish_reason_t reason = ik_openai_responses_stream_get_finish_reason(ctx);

    ck_assert_int_eq(reason, IK_FINISH_UNKNOWN);
}

END_TEST
/* ================================================================
 * Write Callback Tests
 * ================================================================ */

START_TEST(test_write_callback_returns_total_bytes) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Simple SSE data */
    char data[] = "event: response.created\ndata: {\"response\":{\"model\":\"gpt-4o\"}}\n\n";
    size_t len = strlen(data);

    size_t result = ik_openai_responses_stream_write_callback(
        data, 1, len, ctx);

    ck_assert_uint_eq(result, len);
}

END_TEST

START_TEST(test_write_callback_processes_complete_event) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Complete SSE event for response.created */
    char data[] = "event: response.created\n"
                  "data: {\"response\":{\"model\":\"gpt-4o\"}}\n\n";

    ik_openai_responses_stream_write_callback(data, 1, strlen(data), ctx);

    /* Should emit START event */
    ck_assert_int_eq((int)events->count, 1);
    ck_assert_int_eq(events->items[0].type, IK_STREAM_START);
    ck_assert_str_eq(events->items[0].data.start.model, "gpt-4o");
}

END_TEST

START_TEST(test_write_callback_processes_text_delta) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Set up model first */
    char created[] = "event: response.created\n"
                     "data: {\"response\":{\"model\":\"gpt-4o\"}}\n\n";
    ik_openai_responses_stream_write_callback(created, 1, strlen(created), ctx);

    /* Text delta */
    char delta[] = "event: response.output_text.delta\n"
                   "data: {\"delta\":\"Hello\",\"content_index\":0}\n\n";
    ik_openai_responses_stream_write_callback(delta, 1, strlen(delta), ctx);

    /* Should have START + TEXT_DELTA */
    ck_assert_int_eq((int)events->count, 2);
    ck_assert_int_eq(events->items[1].type, IK_STREAM_TEXT_DELTA);
    ck_assert_str_eq(events->items[1].data.delta.text, "Hello");
    ck_assert_int_eq(events->items[1].index, 0);
}

END_TEST

START_TEST(test_write_callback_handles_chunked_data) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Send event in chunks */
    char chunk1[] = "event: response.created\n";
    char chunk2[] = "data: {\"response\":{\"model\":\"gpt-4o\"}}\n";
    char chunk3[] = "\n";

    ik_openai_responses_stream_write_callback(chunk1, 1, strlen(chunk1), ctx);
    ik_openai_responses_stream_write_callback(chunk2, 1, strlen(chunk2), ctx);
    ik_openai_responses_stream_write_callback(chunk3, 1, strlen(chunk3), ctx);

    /* Should emit START event once complete event is parsed */
    ck_assert_int_eq((int)events->count, 1);
    ck_assert_int_eq(events->items[0].type, IK_STREAM_START);
}

END_TEST

START_TEST(test_write_callback_handles_multiple_events) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Multiple events in one chunk */
    char data[] =
        "event: response.created\n"
        "data: {\"response\":{\"model\":\"gpt-4o\"}}\n\n"
        "event: response.output_text.delta\n"
        "data: {\"delta\":\"Hi\",\"content_index\":0}\n\n";

    ik_openai_responses_stream_write_callback(data, 1, strlen(data), ctx);

    /* Should emit START + TEXT_DELTA */
    ck_assert_int_eq((int)events->count, 2);
    ck_assert_int_eq(events->items[0].type, IK_STREAM_START);
    ck_assert_int_eq(events->items[1].type, IK_STREAM_TEXT_DELTA);
}

END_TEST

START_TEST(test_write_callback_skips_event_with_null_event_name) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* SSE event without event field (data-only) */
    char data[] = "data: {\"response\":{\"model\":\"gpt-4o\"}}\n\n";

    ik_openai_responses_stream_write_callback(data, 1, strlen(data), ctx);

    /* Should not emit any events */
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_write_callback_skips_event_with_null_data) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* SSE event without data field */
    char data[] = "event: response.created\n\n";

    ik_openai_responses_stream_write_callback(data, 1, strlen(data), ctx);

    /* Should not emit any events */
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_write_callback_with_size_greater_than_one) {
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    char data[] = "event: response.created\n"
                  "data: {\"response\":{\"model\":\"gpt-4o\"}}\n\n";
    size_t len = strlen(data);

    /* Simulate size=2, nmemb=len/2 */
    size_t result = ik_openai_responses_stream_write_callback(
        data, 2, len / 2, ctx);

    ck_assert_uint_eq(result, len / 2 * 2);
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *openai_streaming_responses_suite(void)
{
    Suite *s = suite_create("OpenAI Streaming Responses");

    /* Context Creation and Getters */
    TCase *tc_ctx = tcase_create("Context");
    tcase_set_timeout(tc_ctx, 30);
    tcase_add_checked_fixture(tc_ctx, setup, teardown);
    tcase_add_test(tc_ctx, test_ctx_create_initializes_correctly);
    tcase_add_test(tc_ctx, test_get_usage_returns_zero_initially);
    tcase_add_test(tc_ctx, test_get_finish_reason_returns_unknown_initially);
    suite_add_tcase(s, tc_ctx);

    /* Write Callback */
    TCase *tc_write = tcase_create("WriteCallback");
    tcase_set_timeout(tc_write, 30);
    tcase_add_checked_fixture(tc_write, setup, teardown);
    tcase_add_test(tc_write, test_write_callback_returns_total_bytes);
    tcase_add_test(tc_write, test_write_callback_processes_complete_event);
    tcase_add_test(tc_write, test_write_callback_processes_text_delta);
    tcase_add_test(tc_write, test_write_callback_handles_chunked_data);
    tcase_add_test(tc_write, test_write_callback_handles_multiple_events);
    tcase_add_test(tc_write, test_write_callback_skips_event_with_null_event_name);
    tcase_add_test(tc_write, test_write_callback_skips_event_with_null_data);
    tcase_add_test(tc_write, test_write_callback_with_size_greater_than_one);
    suite_add_tcase(s, tc_write);

    return s;
}

int main(void)
{
    Suite *s = openai_streaming_responses_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
