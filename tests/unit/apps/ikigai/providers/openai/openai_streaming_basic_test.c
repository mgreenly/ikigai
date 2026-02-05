#include "tests/test_constants.h"
/**
 * @file openai_streaming_basic_test.c
 * @brief Basic OpenAI streaming tests - initial deltas and content accumulation
 *
 * Tests SSE parsing, delta accumulation, and event normalization for basic text streaming.
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "apps/ikigai/providers/openai/streaming.h"
#include "apps/ikigai/providers/openai/openai.h"
#include "apps/ikigai/providers/provider.h"

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
 * Basic Streaming Tests
 * ================================================================ */

START_TEST(test_parse_initial_role_delta) {
    /* Create streaming context */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Process initial chunk with role and model */
    const char *data = "{\"id\":\"chatcmpl-123\",\"model\":\"gpt-4\","
                       "\"choices\":[{\"delta\":{\"role\":\"assistant\"},\"index\":0}]}";
    ik_openai_chat_stream_process_data(sctx, data);

    /* First delta with role should not emit START yet (waits for content) */
    ck_assert_int_eq((int)events->count, 0);
}
END_TEST

START_TEST(test_parse_content_delta) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Process initial chunk to set model */
    const char *init_data = "{\"id\":\"chatcmpl-123\",\"model\":\"gpt-4\","
                            "\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}";
    ik_openai_chat_stream_process_data(sctx, init_data);

    /* Process content delta */
    const char *content_data = "{\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}";
    ik_openai_chat_stream_process_data(sctx, content_data);

    /* Should emit START + TEXT_DELTA */
    ck_assert_int_eq((int)events->count, 2);
    ck_assert_int_eq(events->items[0].type, IK_STREAM_START);
    ck_assert_str_eq(events->items[0].data.start.model, "gpt-4");
    ck_assert_int_eq(events->items[1].type, IK_STREAM_TEXT_DELTA);
    ck_assert_str_eq(events->items[1].data.delta.text, "Hello");
}

END_TEST

START_TEST(test_parse_finish_reason) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Set model first */
    const char *init_data = "{\"model\":\"gpt-4\",\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}";
    ik_openai_chat_stream_process_data(sctx, init_data);

    /* Process finish_reason */
    const char *finish_data = "{\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}]}";
    ik_openai_chat_stream_process_data(sctx, finish_data);

    /* Finish reason updates internal state but doesn't emit event yet */
    ik_finish_reason_t reason = ik_openai_chat_stream_get_finish_reason(sctx);
    ck_assert_int_eq(reason, IK_FINISH_STOP);
}

END_TEST

START_TEST(test_handle_done_marker) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Set model and finish reason */
    const char *init_data = "{\"model\":\"gpt-4\",\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}";
    ik_openai_chat_stream_process_data(sctx, init_data);

    const char *finish_data = "{\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}]}";
    ik_openai_chat_stream_process_data(sctx, finish_data);

    /* Process [DONE] marker */
    ik_openai_chat_stream_process_data(sctx, "[DONE]");

    /* Should emit DONE event */
    ck_assert_int_eq((int)events->count, 1);
    ck_assert_int_eq(events->items[0].type, IK_STREAM_DONE);
    ck_assert_int_eq(events->items[0].data.done.finish_reason, IK_FINISH_STOP);
}

END_TEST
/* ================================================================
 * Content Accumulation Tests
 * ================================================================ */

START_TEST(test_accumulate_multiple_deltas) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Set model */
    const char *init_data = "{\"model\":\"gpt-4\",\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}";
    ik_openai_chat_stream_process_data(sctx, init_data);

    /* Process multiple content deltas */
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}");
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"content\":\" \"}}]}");
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"content\":\"world\"}}]}");

    /* Should emit START + 3 TEXT_DELTA events */
    ck_assert_int_eq((int)events->count, 4);
    ck_assert_int_eq(events->items[0].type, IK_STREAM_START);
    ck_assert_int_eq(events->items[1].type, IK_STREAM_TEXT_DELTA);
    ck_assert_str_eq(events->items[1].data.delta.text, "Hello");
    ck_assert_int_eq(events->items[2].type, IK_STREAM_TEXT_DELTA);
    ck_assert_str_eq(events->items[2].data.delta.text, " ");
    ck_assert_int_eq(events->items[3].type, IK_STREAM_TEXT_DELTA);
    ck_assert_str_eq(events->items[3].data.delta.text, "world");
}

END_TEST

START_TEST(test_handle_empty_content_delta) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Set model */
    const char *init_data = "{\"model\":\"gpt-4\",\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}";
    ik_openai_chat_stream_process_data(sctx, init_data);

    /* Process empty delta */
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{}}]}");

    /* Empty delta should not emit any events (no START since no content yet) */
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

START_TEST(test_preserve_text_order) {
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(
        test_ctx, stream_cb, events);

    /* Set model */
    const char *init_data = "{\"model\":\"gpt-4\",\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}";
    ik_openai_chat_stream_process_data(sctx, init_data);

    /* Process deltas in specific order */
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"content\":\"A\"}}]}");
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"content\":\"B\"}}]}");
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"content\":\"C\"}}]}");

    /* Verify order is preserved */
    ck_assert_int_eq((int)events->count, 4); /* START + 3 deltas */
    ck_assert_str_eq(events->items[1].data.delta.text, "A");
    ck_assert_str_eq(events->items[2].data.delta.text, "B");
    ck_assert_str_eq(events->items[3].data.delta.text, "C");
}

END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *openai_streaming_basic_suite(void)
{
    Suite *s = suite_create("OpenAI Streaming Basic");

    /* Basic Streaming */
    TCase *tc_basic = tcase_create("Basic");
    tcase_set_timeout(tc_basic, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_basic, setup, teardown);
    tcase_add_test(tc_basic, test_parse_initial_role_delta);
    tcase_add_test(tc_basic, test_parse_content_delta);
    tcase_add_test(tc_basic, test_parse_finish_reason);
    tcase_add_test(tc_basic, test_handle_done_marker);
    suite_add_tcase(s, tc_basic);

    /* Content Accumulation */
    TCase *tc_accumulation = tcase_create("Accumulation");
    tcase_set_timeout(tc_accumulation, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_accumulation, setup, teardown);
    tcase_add_test(tc_accumulation, test_accumulate_multiple_deltas);
    tcase_add_test(tc_accumulation, test_handle_empty_content_delta);
    tcase_add_test(tc_accumulation, test_preserve_text_order);
    suite_add_tcase(s, tc_accumulation);

    return s;
}

int main(void)
{
    Suite *s = openai_streaming_basic_suite();
    SRunner *sr = srunner_create(s);
    srunner_set_xml(sr, "reports/check/unit/apps/ikigai/providers/openai/openai_streaming_basic_test.xml");

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
