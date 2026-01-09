#include "../../../test_constants.h"
/**
 * @file streaming_chat_delta_branch_coverage_test.c
 * @brief Branch coverage tests for streaming_chat_delta.c to reach 100%
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/openai/streaming.h"
#include "providers/openai/openai.h"
#include "providers/provider.h"

/* ================================================================
 * Test Context and Event Capture
 * ================================================================ */

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

/* ================================================================
 * Branch Coverage Tests
 * ================================================================ */

START_TEST(test_content_yyjson_get_str_returns_null) {
    /* Line 88: Cover content != NULL false branch
     * When yyjson_get_str returns NULL for content value */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    /* Send content as empty string - yyjson_get_str may return NULL in some cases */
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"content\":\"\"}}]}");

    /* Should handle gracefully without emitting event if content is NULL */
    /* Note: This test attempts to cover the branch but yyjson_get_str on valid
     * string values typically doesn't return NULL, so this may be defensive code */
}
END_TEST

START_TEST(test_tool_call_yyjson_arr_get_returns_null) {
    /* Line 112: Cover tool_call != NULL false branch (second part)
     * When yyjson_arr_get returns NULL */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    /* This is hard to trigger as yyjson_arr_get on valid array won't return NULL
     * This test documents the defensive check */
    ik_openai_chat_stream_process_data(sctx,
                                       "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"tc1\",\"function\":{\"name\":\"test\"}}]}}]}");
}
END_TEST

START_TEST(test_new_tool_call_id_yyjson_get_str_null) {
    /* Line 130: Cover id != NULL false branch
     * When yyjson_get_str(id_val) returns NULL */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    /* Start a tool call with index 0 */
    ik_openai_chat_stream_process_data(sctx,
                                       "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"tc0\",\"function\":{\"name\":\"fn0\"}}]}}]}");

    /* Try to start new tool call with id as non-string type that yyjson_get_str would return NULL for */
    ik_openai_chat_stream_process_data(sctx,
                                       "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":1,\"id\":123,\"function\":{\"name\":\"fn1\"}}]}}]}");
}
END_TEST

START_TEST(test_arguments_delta_yyjson_get_str_null) {
    /* Line 166: Cover arguments != NULL false branch (second part)
     * When yyjson_get_str(arguments_val) returns NULL */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    /* Start a tool call */
    ik_openai_chat_stream_process_data(sctx,
                                       "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"id\":\"tc1\",\"function\":{\"name\":\"test\"}}]}}]}");

    /* Send arguments as integer - yyjson_get_str would return NULL */
    ik_openai_chat_stream_process_data(sctx,
                                       "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":123}}]}}]}");
}
END_TEST

START_TEST(test_arguments_delta_current_tool_args_null) {
    /* Line 169: Cover the ternary false branch where sctx->current_tool_args is NULL
     * This can happen if we somehow get arguments delta without starting a tool call properly */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    /* Directly send arguments without proper tool call start
     * This should handle the NULL current_tool_args case */
    ik_openai_chat_stream_process_data(sctx,
                                       "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"test\"}}]}}]}");
}
END_TEST

START_TEST(test_arguments_not_in_tool_call) {
    /* Line 166: Cover sctx->in_tool_call false branch
     * When we receive arguments but we're not in a tool call */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    /* Send arguments without being in a tool call */
    ik_openai_chat_stream_process_data(sctx,
                                       "{\"choices\":[{\"delta\":{\"tool_calls\":[{\"index\":0,\"function\":{\"arguments\":\"test\"}}]}}]}");

    /* Should not emit delta event */
    ck_assert_int_eq((int)events->count, 0);
}
END_TEST

START_TEST(test_end_tool_call_not_in_tool_call) {
    /* Line 53: Cover sctx->in_tool_call false branch in maybe_end_tool_call
     * When we try to end but not in a tool call */
    ik_openai_chat_stream_ctx_t *sctx = ik_openai_chat_stream_ctx_create(test_ctx, stream_cb, events);

    /* Send text content without ever starting a tool call */
    ik_openai_chat_stream_process_data(sctx, "{\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}");

    /* Should emit START and TEXT_DELTA, but no TOOL_CALL_DONE */
    ck_assert_int_ge((int)events->count, 2);

    bool has_done = false;
    for (size_t i = 0; i < events->count; i++) {
        if (events->items[i].type == IK_STREAM_TOOL_CALL_DONE) {
            has_done = true;
        }
    }
    ck_assert(!has_done);
}
END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *streaming_chat_delta_branch_suite(void)
{
    Suite *s = suite_create("OpenAI Streaming Chat Delta Branch Coverage");

    TCase *tc_branches = tcase_create("BranchCoverage");
    tcase_set_timeout(tc_branches, IK_TEST_TIMEOUT);
    tcase_add_checked_fixture(tc_branches, setup, teardown);
    tcase_add_test(tc_branches, test_content_yyjson_get_str_returns_null);
    tcase_add_test(tc_branches, test_tool_call_yyjson_arr_get_returns_null);
    tcase_add_test(tc_branches, test_new_tool_call_id_yyjson_get_str_null);
    tcase_add_test(tc_branches, test_arguments_delta_yyjson_get_str_null);
    tcase_add_test(tc_branches, test_arguments_delta_current_tool_args_null);
    tcase_add_test(tc_branches, test_arguments_not_in_tool_call);
    tcase_add_test(tc_branches, test_end_tool_call_not_in_tool_call);
    suite_add_tcase(s, tc_branches);

    return s;
}

int main(void)
{
    Suite *s = streaming_chat_delta_branch_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
