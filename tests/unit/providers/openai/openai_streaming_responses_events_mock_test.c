/**
 * @file openai_streaming_responses_events_mock_test.c
 * @brief Tests for OpenAI Responses API event processing with yyjson_doc_get_root mock
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/openai/streaming.h"
#include "providers/openai/streaming_responses_internal.h"
#include "providers/provider.h"
#include "wrapper_json.h"
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

yyjson_val *yyjson_doc_get_root_(yyjson_doc *doc)
{
    (void)doc;
    return NULL;
}

START_TEST(test_yyjson_doc_get_root_returns_null)
{
    ik_openai_responses_stream_ctx_t *ctx = ik_openai_responses_stream_ctx_create(
        test_ctx, stream_cb, events);

    ik_openai_responses_stream_process_event(ctx, "response.created", "{\"response\":{\"model\":\"gpt-4\"}}");
    ck_assert_int_eq((int)events->count, 0);
}

END_TEST

static Suite *openai_streaming_responses_events_mock_suite(void)
{
    Suite *s = suite_create("OpenAI Streaming Responses Events Mock");

    TCase *tc = tcase_create("Mock");
    tcase_add_checked_fixture(tc, setup, teardown);
    tcase_add_test(tc, test_yyjson_doc_get_root_returns_null);
    suite_add_tcase(s, tc);

    return s;
}

int main(void)
{
    Suite *s = openai_streaming_responses_events_mock_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
