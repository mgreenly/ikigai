/**
 * @file google_streaming_foreach_test.c
 * @brief Tests for array iteration branches in Google streaming
 *
 * Tests different array iteration scenarios to cover loop branches.
 */

#include <check.h>
#include <talloc.h>
#include <string.h>
#include "providers/google/streaming.h"
#include "providers/google/response.h"
#include "providers/provider.h"

/* Test context */
static TALLOC_CTX *test_ctx;

/* Captured stream events */
#define MAX_EVENTS 50
#define MAX_STRING_LEN 512
static ik_stream_event_t captured_events[MAX_EVENTS];
static char captured_strings1[MAX_EVENTS][MAX_STRING_LEN];
static char captured_strings2[MAX_EVENTS][MAX_STRING_LEN];
static size_t captured_count;

/* ================================================================
 * Test Callbacks
 * ================================================================ */

static res_t test_stream_cb(const ik_stream_event_t *event, void *ctx)
{
    (void)ctx;

    if (captured_count < MAX_EVENTS) {
        captured_events[captured_count] = *event;

        switch (event->type) {
            case IK_STREAM_TEXT_DELTA:
                if (event->data.delta.text) {
                    strncpy(captured_strings1[captured_count], event->data.delta.text, MAX_STRING_LEN - 1);
                    captured_strings1[captured_count][MAX_STRING_LEN - 1] = '\0';
                    captured_events[captured_count].data.delta.text = captured_strings1[captured_count];
                }
                break;
            case IK_STREAM_THINKING_DELTA:
                if (event->data.delta.text) {
                    strncpy(captured_strings1[captured_count], event->data.delta.text, MAX_STRING_LEN - 1);
                    captured_strings1[captured_count][MAX_STRING_LEN - 1] = '\0';
                    captured_events[captured_count].data.delta.text = captured_strings1[captured_count];
                }
                break;
            case IK_STREAM_TOOL_CALL_START:
                if (event->data.tool_start.id) {
                    strncpy(captured_strings1[captured_count], event->data.tool_start.id, MAX_STRING_LEN - 1);
                    captured_strings1[captured_count][MAX_STRING_LEN - 1] = '\0';
                    captured_events[captured_count].data.tool_start.id = captured_strings1[captured_count];
                }
                if (event->data.tool_start.name) {
                    strncpy(captured_strings2[captured_count], event->data.tool_start.name, MAX_STRING_LEN - 1);
                    captured_strings2[captured_count][MAX_STRING_LEN - 1] = '\0';
                    captured_events[captured_count].data.tool_start.name = captured_strings2[captured_count];
                }
                break;
            default:
                break;
        }

        captured_count++;
    }

    return OK(NULL);
}

/* ================================================================
 * Test Fixtures
 * ================================================================ */

static void setup(void)
{
    test_ctx = talloc_new(NULL);
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * Helper Functions
 * ================================================================ */

static void process_chunk(ik_google_stream_ctx_t *sctx, const char *chunk)
{
    ik_google_stream_process_data(sctx, chunk);
}

static size_t count_events(ik_stream_event_type_t type)
{
    size_t count = 0;
    for (size_t i = 0; i < captured_count; i++) {
        if (captured_events[i].type == type) {
            count++;
        }
    }
    return count;
}

/* ================================================================
 * Array Iteration Coverage Tests
 * ================================================================ */

START_TEST(test_parts_array_with_multiple_items) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Reset to focus on content events */
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));

    /* Process parts array with 3 items to ensure loop iterates multiple times */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":["
        "{\"text\":\"First\"},"
        "{\"text\":\"Second\"},"
        "{\"text\":\"Third\"}"
        "]}}]}";
    process_chunk(sctx, chunk);

    /* Verify multiple TEXT_DELTA events */
    ck_assert_int_eq((int)count_events(IK_STREAM_TEXT_DELTA), 3);
}
END_TEST

START_TEST(test_parts_array_mixed_content) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Reset to focus on content events */
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));

    /* Process parts with mix of text, function calls, and skipped items */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":["
        "{\"text\":\"Hello\"},"
        "{\"functionCall\":{\"name\":\"tool\",\"args\":{}}},"
        "{\"text\":\"\"},"  /* empty, should be skipped */
        "{\"text\":\"World\"}"
        "]}}]}";
    process_chunk(sctx, chunk);

    /* Verify correct events emitted */
    ck_assert_int_eq((int)count_events(IK_STREAM_TEXT_DELTA), 2);
    ck_assert_int_eq((int)count_events(IK_STREAM_TOOL_CALL_START), 1);
}
END_TEST

START_TEST(test_parts_with_all_skipped_items) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Reset to focus on content events */
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));

    /* Process parts where all items are skipped (no text or functionCall) */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":["
        "{\"otherField\":\"value1\"},"
        "{\"text\":null},"
        "{\"text\":\"\"}"
        "]}}]}";
    process_chunk(sctx, chunk);

    /* Verify no content events emitted */
    ck_assert_int_eq((int)captured_count, 0);
}
END_TEST

START_TEST(test_parts_array_with_single_item) {
    ik_google_stream_ctx_t *sctx = NULL;
    res_t r = ik_google_stream_ctx_create(test_ctx, test_stream_cb, NULL, &sctx);
    ck_assert(!is_err(&r));

    /* Process START first */
    process_chunk(sctx, "{\"modelVersion\":\"gemini-2.5-flash\"}");

    /* Reset to focus on content events */
    captured_count = 0;
    memset(captured_events, 0, sizeof(captured_events));

    /* Process parts array with exactly 1 item */
    const char *chunk =
        "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Solo\"}]}}]}";
    process_chunk(sctx, chunk);

    /* Verify single TEXT_DELTA event */
    ck_assert_int_eq((int)count_events(IK_STREAM_TEXT_DELTA), 1);
}
END_TEST

/* ================================================================
 * Test Suite Setup
 * ================================================================ */

static Suite *google_streaming_foreach_suite(void)
{
    Suite *s = suite_create("Google Streaming - Array Iteration");

    TCase *tc_main = tcase_create("Parts Array Iteration");
    tcase_set_timeout(tc_main, 30);
    tcase_add_checked_fixture(tc_main, setup, teardown);
    tcase_add_test(tc_main, test_parts_array_with_multiple_items);
    tcase_add_test(tc_main, test_parts_array_mixed_content);
    tcase_add_test(tc_main, test_parts_with_all_skipped_items);
    tcase_add_test(tc_main, test_parts_array_with_single_item);
    suite_add_tcase(s, tc_main);

    return s;
}

int main(void)
{
    Suite *s = google_streaming_foreach_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
